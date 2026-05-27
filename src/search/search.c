#include "search/search.h"
#include "movegen/movegen.h"
#include "eval/eval.h"
#include "uci/uci.h"
#include "transposition_table.h"
#include "zobrist.h"
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#define MAX_DEPTH 64
#define INFINITY_SCORE 30000
#define MATE_SCORE 29000

typedef struct {
    int length;
    Move moves[MAX_DEPTH];
} PVLine;

#include <math.h>

static FILE *search_log_output = NULL;
static int stop_requested = 0;
static unsigned hash_size = 16; /* default 16MB */
static uint64_t node_count = 0;

static Move killer_moves[2][MAX_DEPTH];

static int lmr_reductions[MAX_DEPTH][MAX_MOVES];
static int lmr_initialized = 0;

static void init_lmr(void) {
    for (int depth = 0; depth < MAX_DEPTH; depth++) {
        for (int moves = 0; moves < MAX_MOVES; moves++) {
            if (depth == 0 || moves == 0) {
                lmr_reductions[depth][moves] = 0;
                continue;
            }
            double r = 0.5 + log((double)depth) * log((double)moves) / 3.0;
            lmr_reductions[depth][moves] = (int)r;
        }
    }
    lmr_initialized = 1;
}

static TranspositionTable tt;
static int tt_initialized = 0;

typedef struct UndoNode {
    const Undo *undo;
    const struct UndoNode *parent;
} UndoNode;

/* Forward Declarations */
static int pvs(Position *pos, int depth, int ply, int alpha, int beta, PVLine *pv, uint64_t start_time, const SearchLimits *limits, Move pv_move, const UndoNode *history, int allow_nmp);

FILE *search_set_log_output(FILE *output) {
    FILE *prev = search_log_output;
    search_log_output = output;
    return prev;
}

void search_reset_stop_request(void) {
    stop_requested = 0;
}

void search_request_stop(void) {
    stop_requested = 1;
}

int search_set_hash_size_mb(unsigned size_mb) {
    hash_size = size_mb;
    if (tt_initialized) {
        transposition_table_free(&tt);
        tt_initialized = 0;
    }
    if (transposition_table_init(&tt, (size_t)hash_size * 1024 * 1024) == 0) {
        tt_initialized = 1;
    }
    return 0;
}

int search_compute_time_limits(const Position *board,
                               unsigned depth,
                               unsigned remaining_ms,
                               unsigned increment_ms,
                               SearchLimits *limits) {
    (void)board;
    (void)depth;
    limits->soft_time_limit_ms = remaining_ms / 20;
    limits->hard_time_limit_ms = remaining_ms / 10 + increment_ms;
    return 0;
}

/* Helper to get time in milliseconds */
static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

/* Helper to check for basic draw states */
static int is_draw(const Position *pos) {
    if (pos->fiftyMoveCounter >= 100) {
        return 1;
    }
    return 0;
}

/* Helper to check for repetition within the current search path */
static int is_repetition(const Position *pos, const UndoNode *history) {
    uint64_t current_hash = pos->hashKey;
    const UndoNode *curr = history;
    while (curr != NULL) {
        if (curr->undo->old_hash == current_hash) {
            return 1;
        }
        PieceType pt = PIECE_TYPE(curr->undo->moving);
        if (pt == PAWN || curr->undo->captured != EMPTY) {
            break;
        }
        curr = curr->parent;
    }
    return 0;
}

static const int piece_values[8] = {
    0,     // NONE
    100,   // PAWN
    320,   // KNIGHT
    330,   // BISHOP
    500,   // ROOK
    900,   // QUEEN
    20000, // KING
    0
};

/* Helper to get piece values for MVV-LVA move sorting */
static inline int get_piece_value(PieceType pt) {
    return piece_values[pt];
}

/* Check if move is capture or promotion */
static int move_is_capture_or_promo(const Position *pos, Move m) {
    int to = MOVE_TO(m);
    if (pos->board[to] != EMPTY) {
        return 1;
    }
    int from = MOVE_FROM(m);
    Piece moving_piece = pos->board[from];
    PieceType pt = PIECE_TYPE(moving_piece);
    if (pt == PAWN && to == pos->enPassantSquare) {
        return 1;
    }
    if (pt == PAWN && (RANK_OF(to) == 0 || RANK_OF(to) == 7)) {
        return 1;
    }
    return 0;
}

/* Scopes a move for ordering */
static int score_move(const Position *pos, Move m, Move pv_move, int ply) {
    if (m == pv_move) {
        return 1000000;
    }

    int from = MOVE_FROM(m);
    int to = MOVE_TO(m);
    Piece moving_piece = pos->board[from];
    PieceType attacker = PIECE_TYPE(moving_piece);

    // Is it a capture?
    Piece victim_piece = pos->board[to];
    int is_cap = (victim_piece != EMPTY);
    if (!is_cap && attacker == PAWN && to == pos->enPassantSquare) {
        victim_piece = MAKE_PIECE(OPPOSITE(pos->sideToMove), PAWN);
        is_cap = 1;
    }

    if (is_cap) {
        PieceType victim = PIECE_TYPE(victim_piece);
        // MVV-LVA: Most Valuable Victim, Least Valuable Attacker
        return 100000 + get_piece_value(victim) * 10 - get_piece_value(attacker);
    }

    // Promotions
    if (attacker == PAWN && (RANK_OF(to) == 0 || RANK_OF(to) == 7)) {
        return 50000 + MOVE_PROMO(m);
    }

    // Killer moves
    if (m == killer_moves[0][ply]) {
        return 90000;
    }
    if (m == killer_moves[1][ply]) {
        return 80000;
    }

    // Castling moves
    int flag = MOVE_FLAG(m);
    if (flag == MOVE_CASTLE_KS || flag == MOVE_CASTLE_QS) {
        return 10000;
    }

    return 0;
}

/* Lazy/incremental move selection: picks the best move from the remaining unsorted range */
static inline void pick_best_move(Move *moves, int *scores, int count, int current_idx) {
    int best_idx = current_idx;
    for (int j = current_idx + 1; j < count; j++) {
        if (scores[j] > scores[best_idx]) {
            best_idx = j;
        }
    }
    if (best_idx != current_idx) {
        Move temp_move = moves[current_idx];
        moves[current_idx] = moves[best_idx];
        moves[best_idx] = temp_move;

        int temp_score = scores[current_idx];
        scores[current_idx] = scores[best_idx];
        scores[best_idx] = temp_score;
    }
}

/* Helper to increment node count and check time limits */
static inline void check_time_limit(uint64_t start_time, const SearchLimits *limits) {
    node_count++;
    if (node_count % 2048 == 0) {
        if (limits->hard_time_limit_ms > 0) {
            uint64_t elapsed = get_time_ms() - start_time;
            if (elapsed >= limits->hard_time_limit_ms) {
                stop_requested = 1;
            }
        }
    }
}

/* Checks if a castling move is illegal (king under attack or passing through attack) */
static int is_illegal_castle(const Position *pos, Move m) {
    int flag = MOVE_FLAG(m);
    if (flag == MOVE_CASTLE_KS || flag == MOVE_CASTLE_QS) {
        Color us = pos->sideToMove;
        int king_sq = pos->kingSq[COLOR_IDX(us)];
        if (is_square_attacked(pos, king_sq, OPPOSITE(us))) {
            return 1;
        }
        int step_sq = (flag == MOVE_CASTLE_KS) ?
            ((us == WHITE) ? F1 : F8) :
            ((us == WHITE) ? D1 : D8);
        if (is_square_attacked(pos, step_sq, OPPOSITE(us))) {
            return 1;
        }
    }
    return 0;
}

/* Applies a move and verifies its legality. If illegal, undoes and returns 0. */
static int make_move_and_check_legal(Position *pos, Move m, Undo *u) {
    apply_move(pos, m, u);
    Color us = OPPOSITE(pos->sideToMove); // the side that just moved
    int king_sq = pos->kingSq[COLOR_IDX(us)];
    if (is_square_attacked(pos, king_sq, pos->sideToMove)) {
        undo_move(pos, u);
        return 0; // Illegal
    }
    return 1; // Legal
}

/* Returns checkmate or stalemate score if no moves are legal */
static int evaluate_no_moves(int in_check, int ply) {
    if (in_check) {
        return -MATE_SCORE + ply; // Checkmate
    }
    return 0; // Stalemate
}

/* Null-Move Pruning (NMP) Helper */
static int try_null_move_pruning(Position *pos, int depth, int ply, int beta, uint64_t start_time, const SearchLimits *limits, const UndoNode *history) {
    uint64_t non_pawns = pos->pieces[COLOR_IDX(pos->sideToMove)][KNIGHT] |
                         pos->pieces[COLOR_IDX(pos->sideToMove)][BISHOP] |
                         pos->pieces[COLOR_IDX(pos->sideToMove)][ROOK]   |
                         pos->pieces[COLOR_IDX(pos->sideToMove)][QUEEN];
    if (non_pawns == 0) {
        return -INFINITY_SCORE;
    }

    int R = (depth > 6) ? 3 : 2;

    // Make Null Move
    int old_ep = pos->enPassantSquare;
    int old_fifty = pos->fiftyMoveCounter;
    uint64_t old_hash = pos->hashKey;

    pos->sideToMove = OPPOSITE(pos->sideToMove);
    pos->hashKey ^= zobrist_side_to_move_key();
    if (pos->enPassantSquare != SQ_NONE) {
        pos->hashKey ^= zobrist_en_passant_key(pos->enPassantSquare);
        pos->enPassantSquare = SQ_NONE;
    }
    pos->fiftyMoveCounter++;

    PVLine child_pv;
    child_pv.length = 0;

    // Search null move with a reduced depth and zero window
    int score = -pvs(pos, depth - 1 - R, ply + 1, -beta, -beta + 1, &child_pv, start_time, limits, 0, history, 0);

    // Unmake Null Move
    pos->enPassantSquare = old_ep;
    pos->fiftyMoveCounter = old_fifty;
    pos->sideToMove = OPPOSITE(pos->sideToMove);
    pos->hashKey = old_hash;

    return score;
}

/* Quiescence Search */
static int quiescence(Position *pos, int ply, int alpha, int beta, uint64_t start_time, const SearchLimits *limits) {
    check_time_limit(start_time, limits);
    if (stop_requested) {
        return 0;
    }

    int stand_pat = evaluate(pos);
    if (pos->sideToMove == BLACK) {
        stand_pat = -stand_pat;
    }

    if (stand_pat >= beta) {
        return beta;
    }
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    Move moves[MAX_MOVES];
    int count = 0;
    int in_check = is_square_attacked(pos, pos->kingSq[COLOR_IDX(pos->sideToMove)], OPPOSITE(pos->sideToMove));

    if (in_check) {
        count = movegen_pseudo_legal(pos, moves);
    } else {
        Move all_moves[MAX_MOVES];
        int all_count = movegen_pseudo_legal(pos, all_moves);
        for (int i = 0; i < all_count; i++) {
            if (move_is_capture_or_promo(pos, all_moves[i])) {
                moves[count++] = all_moves[i];
            }
        }
    }

    int scores[MAX_MOVES];
    for (int i = 0; i < count; i++) {
        scores[i] = score_move(pos, moves[i], 0, ply);
    }

    int legal_moves_searched = 0;

    for (int i = 0; i < count; i++) {
        pick_best_move(moves, scores, count, i);
        if (is_illegal_castle(pos, moves[i])) {
            continue;
        }

        Undo u;
        if (!make_move_and_check_legal(pos, moves[i], &u)) {
            continue;
        }

        legal_moves_searched++;
        int score = -quiescence(pos, ply + 1, -beta, -alpha, start_time, limits);
        undo_move(pos, &u);

        if (stop_requested) {
            return 0;
        }

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    if (in_check && legal_moves_searched == 0) {
        return -MATE_SCORE + ply;
    }

    return alpha;
}

/* Principal Variation Search */
static int pvs(Position *pos, int depth, int ply, int alpha, int beta, PVLine *pv, uint64_t start_time, const SearchLimits *limits, Move pv_move, const UndoNode *history, int allow_nmp) {
    check_time_limit(start_time, limits);
    if (stop_requested) {
        return 0;
    }

    if (ply > 0 && (is_draw(pos) || is_repetition(pos, history))) {
        pv->length = 0;
        return 0;
    }

    // Mate distance pruning
    int mated_score = -MATE_SCORE + ply;
    int mate_score = MATE_SCORE - ply;
    if (alpha < mated_score) alpha = mated_score;
    if (beta > mate_score) beta = mate_score;
    if (alpha >= beta) return alpha;

    int old_alpha = alpha;
    TranspositionEntry tt_entry;
    int tt_hit = transposition_table_lookup(&tt, pos->hashKey, ply, &tt_entry);
    if (tt_hit == 1) {
        if (tt_entry.depth >= (unsigned)depth) {
            if (tt_entry.bound == TT_BOUND_EXACT) {
                pv->length = 0;
                if (tt_entry.best_move != 0) {
                    pv->moves[0] = tt_entry.best_move;
                    pv->length = 1;
                }
                return tt_entry.score;
            } else if (tt_entry.bound == TT_BOUND_LOWER && tt_entry.score >= beta) {
                return tt_entry.score;
            } else if (tt_entry.bound == TT_BOUND_UPPER && tt_entry.score <= alpha) {
                return tt_entry.score;
            }
        }
    }

    if (depth <= 0) {
        pv->length = 0;
        return quiescence(pos, ply, alpha, beta, start_time, limits);
    }

    int in_check = is_square_attacked(pos, pos->kingSq[COLOR_IDX(pos->sideToMove)], OPPOSITE(pos->sideToMove));

    // Null Move Pruning (NMP)
    if (allow_nmp && !in_check && depth >= 3) {
        int score = try_null_move_pruning(pos, depth, ply, beta, start_time, limits, history);
        if (stop_requested) {
            return 0;
        }
        if (score >= beta) {
            if (score >= MATE_SCORE - 100) {
                return beta;
            }
            return score;
        }
    }

    Move moves[MAX_MOVES];
    int count = movegen_pseudo_legal(pos, moves);

    Move hash_move = 0;
    if (tt_hit == 1 && tt_entry.best_move != 0) {
        hash_move = tt_entry.best_move;
    } else if (ply == 0) {
        hash_move = pv_move;
    }

    int scores[MAX_MOVES];
    for (int i = 0; i < count; i++) {
        scores[i] = score_move(pos, moves[i], hash_move, ply);
    }

    int best_score = -INFINITY_SCORE;
    Move best_move = 0;
    int search_pv = 1;
    int legal_moves_searched = 0;

    for (int i = 0; i < count; i++) {
        pick_best_move(moves, scores, count, i);
        PVLine child_pv;
        child_pv.length = 0;

        if (is_illegal_castle(pos, moves[i])) {
            continue;
        }

        int reduction = 0;
        int is_pv = (beta - alpha > 1);

        if (depth >= 5 && legal_moves_searched >= 1 && !move_is_capture_or_promo(pos, moves[i]) && !in_check) {
            int move_count = legal_moves_searched + 1;
            reduction = lmr_reductions[depth >= MAX_DEPTH ? MAX_DEPTH - 1 : depth][move_count >= MAX_MOVES ? MAX_MOVES - 1 : move_count];
            if (is_pv) {
                reduction--;
            }
            if (moves[i] == killer_moves[0][ply] || moves[i] == killer_moves[1][ply]) {
                reduction--;
            }
            if (reduction < 0) {
                reduction = 0;
            }
        }

        Undo u;
        if (!make_move_and_check_legal(pos, moves[i], &u)) {
            continue;
        }

        legal_moves_searched++;

        int gives_check = is_square_attacked(pos, pos->kingSq[COLOR_IDX(pos->sideToMove)], OPPOSITE(pos->sideToMove));
        if (gives_check) {
            reduction = 0;
        }

        UndoNode next_node;
        next_node.undo = &u;
        next_node.parent = history;

        int score;
        if (search_pv) {
            score = -pvs(pos, depth - 1, ply + 1, -beta, -alpha, &child_pv, start_time, limits, 0, &next_node, 1);
            search_pv = 0;
        } else {
            if (reduction > 0) {
                score = -pvs(pos, depth - 1 - reduction, ply + 1, -alpha - 1, -alpha, &child_pv, start_time, limits, 0, &next_node, 1);
                if (score > alpha && !stop_requested) {
                    score = -pvs(pos, depth - 1, ply + 1, -alpha - 1, -alpha, &child_pv, start_time, limits, 0, &next_node, 1);
                }
            } else {
                score = -pvs(pos, depth - 1, ply + 1, -alpha - 1, -alpha, &child_pv, start_time, limits, 0, &next_node, 1);
            }

            if (score > alpha && score < beta && !stop_requested) {
                score = -pvs(pos, depth - 1, ply + 1, -beta, -score, &child_pv, start_time, limits, 0, &next_node, 1);
            }
        }

        undo_move(pos, &u);

        if (stop_requested) {
            return 0;
        }

        if (score > best_score) {
            best_score = score;
            best_move = moves[i];
        }

        if (score > alpha) {
            alpha = score;

            pv->moves[0] = moves[i];
            memcpy(pv->moves + 1, child_pv.moves, child_pv.length * sizeof(Move));
            pv->length = child_pv.length + 1;
        }

        if (alpha >= beta) {
            // Save killer move if it is a quiet move
            if (!move_is_capture_or_promo(pos, moves[i])) {
                if (killer_moves[0][ply] != moves[i]) {
                    killer_moves[1][ply] = killer_moves[0][ply];
                    killer_moves[0][ply] = moves[i];
                }
            }
            break;
        }
    }

    if (legal_moves_searched == 0) {
        pv->length = 0;
        best_score = evaluate_no_moves(in_check, ply);
    }

    if (!stop_requested) {
        TranspositionBound bound = TT_BOUND_EXACT;
        if (best_score <= old_alpha) {
            bound = TT_BOUND_UPPER;
        } else if (best_score >= beta) {
            bound = TT_BOUND_LOWER;
        }
        transposition_table_store(&tt, pos->hashKey, depth, ply, best_score, bound, best_move);
    }

    return best_score;
}

/* Aspiration Window Widening Loop */
static int search_aspiration_window(Position *pos, unsigned depth, int previous_score, PVLine *pv, uint64_t start_time, const SearchLimits *limits, Move best_move_so_far) {
    int delta = 40; // centipawns window
    int alpha = previous_score - delta;
    int beta = previous_score + delta;
    int score = previous_score;

    while (1) {
        if (alpha < -INFINITY_SCORE) alpha = -INFINITY_SCORE;
        if (beta > INFINITY_SCORE) beta = INFINITY_SCORE;

        score = pvs(pos, depth, 0, alpha, beta, pv, start_time, limits, best_move_so_far, NULL, 1);

        if (stop_requested) {
            break;
        }

        if (score <= alpha) {
            // Fail low: widen alpha
            alpha -= delta;
            delta *= 2;
        } else if (score >= beta) {
            // Fail high: widen beta
            beta += delta;
            delta *= 2;
        } else {
            break;
        }
    }
    return score;
}

/* Outputs UCI log information for the current depth */
static void log_search_info(unsigned depth, int score, uint64_t nodes, const PVLine *pv, const Position *board) {
    if (!search_log_output) {
        return;
    }
    fprintf(search_log_output, "info depth %u score ", depth);
    if (score > MATE_SCORE - 100) {
        int plies = MATE_SCORE - score;
        int moves_to_mate = (plies + 1) / 2;
        fprintf(search_log_output, "mate %d ", moves_to_mate);
    } else if (score < -MATE_SCORE + 100) {
        int plies = -MATE_SCORE - score;
        int moves_to_mate = (plies - 1) / 2;
        fprintf(search_log_output, "mate %d ", moves_to_mate);
    } else {
        fprintf(search_log_output, "cp %d ", score);
    }
    fprintf(search_log_output, "nodes %llu pv", (unsigned long long)nodes);
    Position temp_pos = *board;
    for (int i = 0; i < pv->length; i++) {
        char move_str[6];
        if (uci_move_to_string(&temp_pos, pv->moves[i], move_str, sizeof(move_str)) == 0) {
            fprintf(search_log_output, " %s", move_str);
        }
        Undo u;
        apply_move(&temp_pos, pv->moves[i], &u);
    }
    fprintf(search_log_output, "\n");
    fflush(search_log_output);
}

int search_best_move_with_limits(const Position *board, const SearchLimits *limits, SearchResult *result) {
    search_reset_stop_request();
    node_count = 0;
    uint64_t start_time = get_time_ms();

    if (!lmr_initialized) {
        init_lmr();
    }

    if (!tt_initialized) {
        if (transposition_table_init(&tt, (size_t)hash_size * 1024 * 1024) == 0) {
            tt_initialized = 1;
        }
    }
    if (tt_initialized) {
        transposition_table_next_generation(&tt);
    }

    memset(killer_moves, 0, sizeof(killer_moves));

    Position pos = *board;
    Move moves[MAX_MOVES];
    int count = movegen_legal(&pos, moves);

    if (count == 0) {
        result->best_move = 0;
        result->score = 0;
        result->has_legal_move = 0;
        result->node_count = 0;
        return 0;
    }

    // Default fallbacks
    result->best_move = moves[0];
    result->has_legal_move = 1;
    result->score = 0;
    result->node_count = 0;

    Move best_move_so_far = moves[0];
    int best_score_so_far = -INFINITY_SCORE;

    unsigned max_search_depth = limits->depth;
    if (max_search_depth == 0 || max_search_depth > 64) {
        max_search_depth = 64;
    }

    // Iterative Deepening
    for (unsigned d = 1; d <= max_search_depth; d++) {
        if (limits->soft_time_limit_ms > 0) {
            uint64_t elapsed = get_time_ms() - start_time;
            if (elapsed >= limits->soft_time_limit_ms) {
                break;
            }
        }

        PVLine pv;
        pv.length = 0;

        int score;
        if (d >= 3 && abs(best_score_so_far) < MATE_SCORE - 100) {
            score = search_aspiration_window(&pos, d, best_score_so_far, &pv, start_time, limits, best_move_so_far);
        } else {
            score = pvs(&pos, d, 0, -INFINITY_SCORE, INFINITY_SCORE, &pv, start_time, limits, best_move_so_far, NULL, 1);
        }

        if (stop_requested) {
            break;
        }

        if (pv.length > 0) {
            best_move_so_far = pv.moves[0];
            best_score_so_far = score;

            result->best_move = best_move_so_far;
            result->score = best_score_so_far;
            result->node_count = node_count;
        }

        log_search_info(d, score, node_count, &pv, board);
    }

    return 0;
}
