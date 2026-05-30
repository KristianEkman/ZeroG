#include "pawn_eval.h"

static const int passed_pawn_mg[8] = { 0, 5, 10, 20, 35, 60, 100, 0 };
static const int passed_pawn_eg[8] = { 0, 8, 15, 30, 60, 110, 180, 0 };

static const int double_pawn_mg = 8;
static const int double_pawn_eg = 12;

static const int isolated_pawn_mg = 5;
static const int isolated_pawn_eg = 8;

static const int isolated_pawn_semi_open_mg = 10;
static const int isolated_pawn_semi_open_eg = 15;

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

int evaluate_pawns(const Position *pos, int is_endgame) {
    int score = 0;

    // Cache pawn bitboards and rook/queen bitboards for fast passed pawn evaluation
    uint64_t w_pawns_copy = pos->pieces[COLOR_IDX(WHITE)][PAWN];
    uint64_t b_pawns_copy = pos->pieces[COLOR_IDX(BLACK)][PAWN];
    uint64_t w_rooks_queens = pos->pieces[COLOR_IDX(WHITE)][ROOK] | pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    uint64_t b_rooks_queens = pos->pieces[COLOR_IDX(BLACK)][ROOK] | pos->pieces[COLOR_IDX(BLACK)][QUEEN];

    // Evaluate White pawns
    uint64_t w_pawns = w_pawns_copy;
    while (w_pawns) {
        int sq = pop_lsb(&w_pawns);
        int item_score = 100 + pawn_table[sq];

        // Isolated pawn evaluation
        if (!(adjacentFilesMask[sq] & w_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & b_pawns_copy);
            if (is_endgame) {
                item_score -= is_semi_open ? isolated_pawn_semi_open_eg : isolated_pawn_eg;
            } else {
                item_score -= is_semi_open ? isolated_pawn_semi_open_mg : isolated_pawn_mg;
            }
        }

        // Doubled pawn evaluation
        if (fileBehindMasks[0][sq] & w_pawns_copy) {
            item_score -= is_endgame ? double_pawn_eg : double_pawn_mg;
        }

        // Passed pawn evaluation
        if (!(passedPawnMasks[COLOR_IDX(WHITE)][sq] & b_pawns_copy)) {
            int rank = RANK_OF(sq);
            int bp = is_endgame ? passed_pawn_eg[rank] : passed_pawn_mg[rank];

            // Defended/Protected by friendly pawn
            if (pawnAttacks[COLOR_IDX(BLACK)][sq] & w_pawns_copy) {
                bp += is_endgame ? 25 : 15;
            }

            // Blocked by any piece
            int front_sq = sq + 8;
            if (pos->occAll & (1ULL << front_sq)) {
                bp = (bp * 7) / 10;
            }

            // Friendly rook/queen behind
            if (fileBehindMasks[COLOR_IDX(WHITE)][sq] & w_rooks_queens) {
                bp += is_endgame ? 40 : 20;
            }

            // Enemy rook/queen behind
            if (fileBehindMasks[COLOR_IDX(WHITE)][sq] & b_rooks_queens) {
                bp -= is_endgame ? 30 : 15;
            }

            item_score += bp;
        }
        score += item_score;
    }

    // Evaluate Black pawns (subtracted from score)
    uint64_t b_pawns = b_pawns_copy;
    while (b_pawns) {
        int sq = pop_lsb(&b_pawns);
        int item_score = 100 + pawn_table[sq ^ 56];

        // Isolated pawn evaluation
        if (!(adjacentFilesMask[sq] & b_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & w_pawns_copy);
            if (is_endgame) {
                item_score -= is_semi_open ? isolated_pawn_semi_open_eg : isolated_pawn_eg;
            } else {
                item_score -= is_semi_open ? isolated_pawn_semi_open_mg : isolated_pawn_mg;
            }
        }

        // Doubled pawn evaluation
        if (fileBehindMasks[1][sq] & b_pawns_copy) {
            item_score -= is_endgame ? double_pawn_eg : double_pawn_mg;
        }

        // Passed pawn evaluation
        if (!(passedPawnMasks[COLOR_IDX(BLACK)][sq] & w_pawns_copy)) {
            int rank = 7 - RANK_OF(sq);
            int bp = is_endgame ? passed_pawn_eg[rank] : passed_pawn_mg[rank];

            // Defended/Protected by friendly pawn
            if (pawnAttacks[COLOR_IDX(WHITE)][sq] & b_pawns_copy) {
                bp += is_endgame ? 25 : 15;
            }

            // Blocked by any piece
            int front_sq = sq - 8;
            if (pos->occAll & (1ULL << front_sq)) {
                bp = (bp * 7) / 10;
            }

            // Friendly rook/queen behind
            if (fileBehindMasks[COLOR_IDX(BLACK)][sq] & b_rooks_queens) {
                bp += is_endgame ? 40 : 20;
            }

            // Enemy rook/queen behind
            if (fileBehindMasks[COLOR_IDX(BLACK)][sq] & w_rooks_queens) {
                bp -= is_endgame ? 30 : 15;
            }

            item_score += bp;
        }
        score -= item_score;
    }

    return score;
}
