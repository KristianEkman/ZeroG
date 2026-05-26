#include "search/search.h"
#include "movegen/movegen.h"
#include "eval/eval.h"
#include "uci/uci.h"
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

static FILE *search_log_output = NULL;
static int stop_requested = 0;
static unsigned hash_size = 16; /* default 16MB */
static uint64_t node_count = 0;

static Move killer_moves[2][MAX_DEPTH];

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
    (void)hash_size;
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

/* Helper to get piece values for MVV-LVA move sorting */
static int get_piece_value(PieceType pt) {
    switch (pt) {
        case PAWN:   return 100;
        case KNIGHT: return 320;
        case BISHOP: return 330;
        case ROOK:   return 500;
        case QUEEN:  return 900;
        case KING:   return 20000;
        default:     return 0;
    }
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

/* Sorts moves in-place */
static void sort_moves(const Position *pos, Move *moves, int count, Move pv_move, int ply) {
    int scores[MAX_MOVES];
    for (int i = 0; i < count; i++) {
        scores[i] = score_move(pos, moves[i], pv_move, ply);
    }

    for (int i = 0; i < count - 1; i++) {
        int best_idx = i;
        for (int j = i + 1; j < count; j++) {
            if (scores[j] > scores[best_idx]) {
                best_idx = j;
            }
        }
        if (best_idx != i) {
            Move temp_move = moves[i];
            moves[i] = moves[best_idx];
            moves[best_idx] = temp_move;

            int temp_score = scores[i];
            scores[i] = scores[best_idx];
            scores[best_idx] = temp_score;
        }
    }
}

/* Quiescence Search */
static int quiescence(Position *pos, int ply, int alpha, int beta, uint64_t start_time, const SearchLimits *limits) {
    if (stop_requested) {
        return 0;
    }

    node_count++;
    if (node_count % 2048 == 0) {
        if (limits->hard_time_limit_ms > 0) {
            uint64_t elapsed = get_time_ms() - start_time;
            if (elapsed >= limits->hard_time_limit_ms) {
                stop_requested = 1;
                return 0;
            }
        }
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

    sort_moves(pos, moves, count, 0, ply);

    int legal_moves_searched = 0;

    for (int i = 0; i < count; i++) {
        Undo u;
        apply_move(pos, moves[i], &u);

        Color us = OPPOSITE(pos->sideToMove);
        int king_sq = pos->kingSq[COLOR_IDX(us)];
        if (is_square_attacked(pos, king_sq, pos->sideToMove)) {
            undo_move(pos, &u);
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
static int pvs(Position *pos, int depth, int ply, int alpha, int beta, PVLine *pv, uint64_t start_time, const SearchLimits *limits, Move pv_move) {
    if (stop_requested) {
        return 0;
    }

    node_count++;
    if (node_count % 2048 == 0) {
        if (limits->hard_time_limit_ms > 0) {
            uint64_t elapsed = get_time_ms() - start_time;
            if (elapsed >= limits->hard_time_limit_ms) {
                stop_requested = 1;
                return 0;
            }
        }
    }

    if (ply > 0 && is_draw(pos)) {
        pv->length = 0;
        return 0;
    }

    // Mate distance pruning
    int mated_score = -MATE_SCORE + ply;
    int mate_score = MATE_SCORE - ply;
    if (alpha < mated_score) alpha = mated_score;
    if (beta > mate_score) beta = mate_score;
    if (alpha >= beta) return alpha;

    if (depth <= 0) {
        pv->length = 0;
        return quiescence(pos, ply, alpha, beta, start_time, limits);
    }

    int in_check = is_square_attacked(pos, pos->kingSq[COLOR_IDX(pos->sideToMove)], OPPOSITE(pos->sideToMove));

    Move moves[MAX_MOVES];
    int count = movegen_pseudo_legal(pos, moves);

    sort_moves(pos, moves, count, pv_move, ply);

    PVLine child_pv;
    child_pv.length = 0;

    int best_score = -INFINITY_SCORE;
    int search_pv = 1;
    int legal_moves_searched = 0;

    for (int i = 0; i < count; i++) {
        Undo u;
        apply_move(pos, moves[i], &u);

        Color us = OPPOSITE(pos->sideToMove);
        int king_sq = pos->kingSq[COLOR_IDX(us)];
        if (is_square_attacked(pos, king_sq, pos->sideToMove)) {
            undo_move(pos, &u);
            continue;
        }

        legal_moves_searched++;

        int score;
        if (search_pv) {
            score = -pvs(pos, depth - 1, ply + 1, -beta, -alpha, &child_pv, start_time, limits, 0);
            search_pv = 0;
        } else {
            score = -pvs(pos, depth - 1, ply + 1, -alpha - 1, -alpha, &child_pv, start_time, limits, 0);
            if (score > alpha && score < beta && !stop_requested) {
                score = -pvs(pos, depth - 1, ply + 1, -beta, -score, &child_pv, start_time, limits, 0);
            }
        }

        undo_move(pos, &u);

        if (stop_requested) {
            return 0;
        }

        if (score > best_score) {
            best_score = score;
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
        if (in_check) {
            return -MATE_SCORE + ply;
        } else {
            return 0; // Stalemate
        }
    }

    return best_score;
}

int search_best_move_with_limits(const Position *board, const SearchLimits *limits, SearchResult *result) {
    search_reset_stop_request();
    node_count = 0;
    uint64_t start_time = get_time_ms();

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

        int score = pvs(&pos, d, 0, -INFINITY_SCORE, INFINITY_SCORE, &pv, start_time, limits, best_move_so_far);

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

        if (search_log_output) {
            fprintf(search_log_output, "info depth %u score ", d);
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
            fprintf(search_log_output, "nodes %llu pv", (unsigned long long)node_count);
            for (int i = 0; i < pv.length; i++) {
                char move_str[6];
                if (uci_move_to_string(board, pv.moves[i], move_str, sizeof(move_str)) == 0) {
                    fprintf(search_log_output, " %s", move_str);
                }
            }
            fprintf(search_log_output, "\n");
            fflush(search_log_output);
        }
    }

    return 0;
}
