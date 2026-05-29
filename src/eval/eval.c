#include "eval.h"
#include <stdio.h>
#include <math.h>

NeuralNetwork *eval_nn = NULL;
bool use_nn = false;

void eval_init(void) {
    int sizes[] = {768, 64, 32, 1};
    eval_nn = nn_init(sizes, 4);
    if (eval_nn) {
        if (nn_load(eval_nn, "nn_weights.bin")) {
            use_nn = true;
            printf("info string Loaded NN weights from nn_weights.bin\n");
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

/* Piece-Square Tables (PST) pre-flipped vertically so that index 0 corresponds to A1 */

static const int pawn_table[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,  // Rank 1
     5, 10, 10,-20,-20, 10, 10,  5,  // Rank 2
     5, -5,-10,  0,  0,-10, -5,  5,  // Rank 3
     0,  0,  0, 20, 20,  0,  0,  0,  // Rank 4
     5,  5, 10, 25, 25, 10,  5,  5,  // Rank 5
    10, 10, 20, 30, 30, 20, 10, 10,  // Rank 6
    50, 50, 50, 50, 50, 50, 50, 50,  // Rank 7
     0,  0,  0,  0,  0,  0,  0,  0   // Rank 8
};

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
        float features[768];
        nn_extract_features(pos, features);
        float output = nn_forward(eval_nn, features);
        int score = (int)roundf(output * 100.0f);
        
        // The network evaluates from the side-to-move's perspective.
        // We must return the evaluation from White's perspective.
        if (pos->sideToMove == BLACK) {
            score = -score;
        }
        return score;
    }

    int score = 0;

    // Endgame detection criteria:
    // 1. Both sides have no queens OR
    // 2. Every side that has a queen has no other pieces, or at most one minor piece (no rooks and <= 1 minor piece)
    int white_queens = bit_count(pos->pieces[COLOR_IDX(WHITE)][QUEEN]);
    int black_queens = bit_count(pos->pieces[COLOR_IDX(BLACK)][QUEEN]);

    int is_endgame = 0;
    if (white_queens == 0 && black_queens == 0) {
        is_endgame = 1;
    } else {
        int white_ok = 1;
        if (white_queens > 0) {
            int white_minors = bit_count(pos->pieces[COLOR_IDX(WHITE)][KNIGHT]) + bit_count(pos->pieces[COLOR_IDX(WHITE)][BISHOP]);
            int white_rooks = bit_count(pos->pieces[COLOR_IDX(WHITE)][ROOK]);
            if (white_rooks > 0 || white_minors > 1) {
                white_ok = 0;
            }
        }
        int black_ok = 1;
        if (black_queens > 0) {
            int black_minors = bit_count(pos->pieces[COLOR_IDX(BLACK)][KNIGHT]) + bit_count(pos->pieces[COLOR_IDX(BLACK)][BISHOP]);
            int black_rooks = bit_count(pos->pieces[COLOR_IDX(BLACK)][ROOK]);
            if (black_rooks > 0 || black_minors > 1) {
                black_ok = 0;
            }
        }
        if (white_ok && black_ok) {
            is_endgame = 1;
        }
    }

    // Evaluate White pieces
    uint64_t w_pawns = pos->pieces[COLOR_IDX(WHITE)][PAWN];
    while (w_pawns) {
        int sq = pop_lsb(&w_pawns);
        score += 100 + pawn_table[sq];
    }
    uint64_t w_knights = pos->pieces[COLOR_IDX(WHITE)][KNIGHT];
    while (w_knights) {
        int sq = pop_lsb(&w_knights);
        score += 320 + knight_table[sq];
    }
    uint64_t w_bishops = pos->pieces[COLOR_IDX(WHITE)][BISHOP];
    while (w_bishops) {
        int sq = pop_lsb(&w_bishops);
        score += 330 + bishop_table[sq];
    }
    uint64_t w_rooks = pos->pieces[COLOR_IDX(WHITE)][ROOK];
    while (w_rooks) {
        int sq = pop_lsb(&w_rooks);
        score += 500 + rook_table[sq];
    }
    uint64_t w_queens = pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    while (w_queens) {
        int sq = pop_lsb(&w_queens);
        score += 900 + queen_table[sq];
    }
    uint64_t w_king = pos->pieces[COLOR_IDX(WHITE)][KING];
    while (w_king) {
        int sq = pop_lsb(&w_king);
        score += 20000 + (is_endgame ? king_end_table[sq] : king_middle_table[sq]);
    }

    // Evaluate Black pieces (subtracted from score)
    uint64_t b_pawns = pos->pieces[COLOR_IDX(BLACK)][PAWN];
    while (b_pawns) {
        int sq = pop_lsb(&b_pawns);
        score -= 100 + pawn_table[sq ^ 56];
    }
    uint64_t b_knights = pos->pieces[COLOR_IDX(BLACK)][KNIGHT];
    while (b_knights) {
        int sq = pop_lsb(&b_knights);
        score -= 320 + knight_table[sq ^ 56];
    }
    uint64_t b_bishops = pos->pieces[COLOR_IDX(BLACK)][BISHOP];
    while (b_bishops) {
        int sq = pop_lsb(&b_bishops);
        score -= 330 + bishop_table[sq ^ 56];
    }
    uint64_t b_rooks = pos->pieces[COLOR_IDX(BLACK)][ROOK];
    while (b_rooks) {
        int sq = pop_lsb(&b_rooks);
        score -= 500 + rook_table[sq ^ 56];
    }
    uint64_t b_queens = pos->pieces[COLOR_IDX(BLACK)][QUEEN];
    while (b_queens) {
        int sq = pop_lsb(&b_queens);
        score -= 900 + queen_table[sq ^ 56];
    }
    uint64_t b_king = pos->pieces[COLOR_IDX(BLACK)][KING];
    while (b_king) {
        int sq = pop_lsb(&b_king);
        score -= 20000 + (is_endgame ? king_end_table[sq ^ 56] : king_middle_table[sq ^ 56]);
    }

    return score;
}
