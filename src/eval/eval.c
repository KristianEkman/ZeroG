#include "eval.h"
#include "eval_mobility.h"
#include "pawn_eval.h"
#include "king_safety.h"
#include "eval_constants.h"
#include <stdio.h>
#include <math.h>

NeuralNetwork *eval_nn = NULL;
bool use_nn = false;

void eval_init(void) {
    int sizes[] = {768, 64, 32, 1};
    eval_nn = nn_init(sizes, 4);
    if (eval_nn) {
        if (nn_load(eval_nn, "nn_weights.bin")) {
            use_nn = false; // Keep default off!
            printf("info string Loaded NN weights from nn_weights.bin (default off)\n");
            fflush(stdout);
        } else {
            printf("info string Warning: Could not load nn_weights.bin, using classical evaluation\n");
            fflush(stdout);
        }
    }
}

void eval_free(void) {
    if (eval_nn) {
        nn_free(eval_nn);
        eval_nn = NULL;
        use_nn = false;
    }
}

#define BISHOP_PAIR_BONUS BISHOP_PAIR_BONUS_VAL

static const int rook_open_file_mg = ROOK_OPEN_FILE_MG_VAL;
static const int rook_open_file_eg = ROOK_OPEN_FILE_EG_VAL;
static const int rook_semi_open_file_mg = ROOK_SEMI_OPEN_FILE_MG_VAL;
static const int rook_semi_open_file_eg = ROOK_SEMI_OPEN_FILE_EG_VAL;

static const uint64_t file_masks[8] = {
    0x0101010101010101ULL << 0,
    0x0101010101010101ULL << 1,
    0x0101010101010101ULL << 2,
    0x0101010101010101ULL << 3,
    0x0101010101010101ULL << 4,
    0x0101010101010101ULL << 5,
    0x0101010101010101ULL << 6,
    0x0101010101010101ULL << 7
};

/* Piece-Square Tables (PST) pre-flipped vertically so that index 0 corresponds to A1 */

static const int knight_table[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,  // Rank 1
    -40,-20,  0,  5,  5,  0,-20,-40,  // Rank 2
    -30,  5, 10, 15, 15, 10,  5,-30,  // Rank 3
    -30,  0, 15, 20, 20, 15,  0,-30,  // Rank 4
    -30,  5, 15, 20, 20, 15,  5,-30,  // Rank 5
    -30,  0, 10, 15, 15, 10,  0,-30,  // Rank 6
    -40,-20,  0,  0,  0,  0,-20,-40,  // Rank 7
    -50,-40,-30,-30,-30,-30,-40,-50   // Rank 8
};

static const int bishop_table[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,  // Rank 1
    -10,  5,  0,  0,  0,  0,  5,-10,  // Rank 2
    -10, 10, 10, 10, 10, 10, 10,-10,  // Rank 3
    -10,  0, 10, 10, 10, 10,  0,-10,  // Rank 4
    -10,  5,  5, 10, 10,  5,  5,-10,  // Rank 5
    -10,  0,  5, 10, 10,  5,  0,-10,  // Rank 6
    -10,  0,  0,  0,  0,  0,  0,-10,  // Rank 7
    -20,-10,-10,-10,-10,-10,-10,-20   // Rank 8
};

static const int rook_table[64] = {
      0,  0,  0,  5,  5,  0,  0,  0,  // Rank 1
     -5,  0,  0,  0,  0,  0,  0, -5,  // Rank 2
     -5,  0,  0,  0,  0,  0,  0, -5,  // Rank 3
     -5,  0,  0,  0,  0,  0,  0, -5,  // Rank 4
     -5,  0,  0,  0,  0,  0,  0, -5,  // Rank 5
     -5,  0,  0,  0,  0,  0,  0, -5,  // Rank 6
      5, 10, 10, 10, 10, 10, 10,  5,  // Rank 7
      0,  0,  0,  0,  0,  0,  0,  0   // Rank 8
};

static const int queen_table[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,  // Rank 1
    -10,  0,  5,  0,  0,  0,  0,-10,  // Rank 2
    -10,  5,  5,  5,  5,  5,  0,-10,  // Rank 3
      0,  0,  5,  5,  5,  5,  0, -5,  // Rank 4
     -5,  0,  5,  5,  5,  5,  0, -5,  // Rank 5
    -10,  0,  5,  5,  5,  5,  0,-10,  // Rank 6
    -10,  0,  0,  0,  0,  0,  0,-10,  // Rank 7
    -20,-10,-10, -5, -5,-10,-10,-20   // Rank 8
};

static const int king_middle_table[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,  // Rank 1
     20, 20,  0,  0,  0,  0, 20, 20,  // Rank 2
    -10,-20,-20,-20,-20,-20,-20,-10,  // Rank 3
    -20,-30,-30,-40,-40,-30,-30,-20,  // Rank 4
    -30,-40,-40,-50,-50,-40,-40,-30,  // Rank 5
    -30,-40,-40,-50,-50,-40,-40,-30,  // Rank 6
    -30,-40,-40,-50,-50,-40,-40,-30,  // Rank 7
    -30,-40,-40,-50,-50,-40,-40,-30   // Rank 8
};

static const int king_end_table[64] = {
    -50,-30,-30,-30,-30,-30,-30,-50,  // Rank 1
    -30,-30,  0,  0,  0,  0,-30,-30,  // Rank 2
    -30,-10, 20, 30, 30, 20,-10,-30,  // Rank 3
    -30,-10, 30, 40, 40, 30,-10,-30,  // Rank 4
    -30,-10, 30, 40, 40, 30,-10,-30,  // Rank 5
    -30,-10, 20, 30, 30, 20,-10,-30,  // Rank 6
    -30,-20,-10,  0,  0,-10,-20,-30,  // Rank 7
    -50,-40,-30,-20,-20,-30,-40,-50   // Rank 8
};

int evaluate(const Position *pos) {
    if (use_nn && eval_nn) {
        int32_t output = nnue_evaluate_accumulator(eval_nn, pos);
        int32_t val = output * 100;
        int score = (val + (val >= 0 ? 4096 : -4096)) / 8192;
        
        // The network evaluates from the side-to-move's perspective.
        // We must return the evaluation from White's perspective.
        if (pos->sideToMove == BLACK) {
            score = -score;
        }
        return score;
    }

    int score = 0;

    // Cache pawn bitboards for fast rook file evaluation
    uint64_t w_pawns_copy = pos->pieces[COLOR_IDX(WHITE)][PAWN];
    uint64_t b_pawns_copy = pos->pieces[COLOR_IDX(BLACK)][PAWN];

    int phase = get_game_phase(pos);
    if (phase > 24) phase = 24;

    // Evaluate White pieces
    score += evaluate_pawns(pos, phase);
    uint64_t w_knights = pos->pieces[COLOR_IDX(WHITE)][KNIGHT];
    while (w_knights) {
        int sq = pop_lsb(&w_knights);
        score += PIECE_KNIGHT_VAL + knight_table[sq];
    }
    uint64_t w_bishops = pos->pieces[COLOR_IDX(WHITE)][BISHOP];
    while (w_bishops) {
        int sq = pop_lsb(&w_bishops);
        score += PIECE_BISHOP_VAL + bishop_table[sq];
    }
    uint64_t w_rooks = pos->pieces[COLOR_IDX(WHITE)][ROOK];
    while (w_rooks) {
        int sq = pop_lsb(&w_rooks);
        int item_score = PIECE_ROOK_VAL + rook_table[sq];

        // Rook on open/semi-open file evaluation
        int file = sq & 7;
        if (!(file_masks[file] & w_pawns_copy)) {
            if (!(file_masks[file] & b_pawns_copy)) {
                item_score += (rook_open_file_mg * phase + rook_open_file_eg * (24 - phase)) / 24;
            } else {
                item_score += (rook_semi_open_file_mg * phase + rook_semi_open_file_eg * (24 - phase)) / 24;
            }
        }

        score += item_score;
    }
    uint64_t w_queens = pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    while (w_queens) {
        int sq = pop_lsb(&w_queens);
        score += PIECE_QUEEN_VAL + queen_table[sq];
    }
    int w_king_sq = pos->kingSq[COLOR_IDX(WHITE)];
    score += 20000 + (king_middle_table[w_king_sq] * phase + king_end_table[w_king_sq] * (24 - phase)) / 24;
    score += evaluate_king_safety(pos, WHITE, phase);

    // Evaluate Black pieces
    uint64_t b_knights = pos->pieces[COLOR_IDX(BLACK)][KNIGHT];
    while (b_knights) {
        int sq = pop_lsb(&b_knights);
        score -= PIECE_KNIGHT_VAL + knight_table[sq ^ 56];
    }
    uint64_t b_bishops = pos->pieces[COLOR_IDX(BLACK)][BISHOP];
    while (b_bishops) {
        int sq = pop_lsb(&b_bishops);
        score -= PIECE_BISHOP_VAL + bishop_table[sq ^ 56];
    }
    uint64_t b_rooks = pos->pieces[COLOR_IDX(BLACK)][ROOK];
    while (b_rooks) {
        int sq = pop_lsb(&b_rooks);
        int item_score = PIECE_ROOK_VAL + rook_table[sq ^ 56];

        // Rook on open/semi-open file evaluation
        int file = sq & 7;
        if (!(file_masks[file] & b_pawns_copy)) {
            if (!(file_masks[file] & w_pawns_copy)) {
                item_score += (rook_open_file_mg * phase + rook_open_file_eg * (24 - phase)) / 24;
            } else {
                item_score += (rook_semi_open_file_mg * phase + rook_semi_open_file_eg * (24 - phase)) / 24;
            }
        }

        score -= item_score;
    }
    uint64_t b_queens = pos->pieces[COLOR_IDX(BLACK)][QUEEN];
    while (b_queens) {
        int sq = pop_lsb(&b_queens);
        score -= PIECE_QUEEN_VAL + queen_table[sq ^ 56];
    }
    int b_king_sq = pos->kingSq[COLOR_IDX(BLACK)];
    score -= 20000 + (king_middle_table[b_king_sq ^ 56] * phase + king_end_table[b_king_sq ^ 56] * (24 - phase)) / 24;
    score -= evaluate_king_safety(pos, BLACK, phase);

    // Bishop pair evaluation
    int w_bishops_count = bit_count(pos->pieces[COLOR_IDX(WHITE)][BISHOP]);
    if (w_bishops_count >= 2) {
        score += BISHOP_PAIR_BONUS;
    }
    int b_bishops_count = bit_count(pos->pieces[COLOR_IDX(BLACK)][BISHOP]);
    if (b_bishops_count >= 2) {
        score -= BISHOP_PAIR_BONUS;
    }

    // Mobility evaluation
    score += evaluate_mobility(pos);

    return score;
}
