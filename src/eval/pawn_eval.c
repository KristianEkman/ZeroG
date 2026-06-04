#include "pawn_eval.h"
#include "eval_constants.h"

static const int passed_pawn_mg[8] = {
    0,
    PASSED_PAWN_MG_R1_VAL,
    PASSED_PAWN_MG_R2_VAL,
    PASSED_PAWN_MG_R3_VAL,
    PASSED_PAWN_MG_R4_VAL,
    PASSED_PAWN_MG_R5_VAL,
    PASSED_PAWN_MG_R6_VAL,
    0
};
static const int passed_pawn_eg[8] = {
    0,
    PASSED_PAWN_EG_R1_VAL,
    PASSED_PAWN_EG_R2_VAL,
    PASSED_PAWN_EG_R3_VAL,
    PASSED_PAWN_EG_R4_VAL,
    PASSED_PAWN_EG_R5_VAL,
    PASSED_PAWN_EG_R6_VAL,
    0
};

static const int double_pawn_mg = DOUBLE_PAWN_MG_VAL;
static const int double_pawn_eg = DOUBLE_PAWN_EG_VAL;

static const int isolated_pawn_mg = ISOLATED_PAWN_MG_VAL;
static const int isolated_pawn_eg = ISOLATED_PAWN_EG_VAL;

static const int isolated_pawn_semi_open_mg = ISOLATED_PAWN_SEMI_OPEN_MG_VAL;
static const int isolated_pawn_semi_open_eg = ISOLATED_PAWN_SEMI_OPEN_EG_VAL;

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

int evaluate_pawns(const Position *pos, int phase) {
    int mg_score = 0;
    int eg_score = 0;

    // Cache pawn bitboards and rook/queen bitboards for fast passed pawn evaluation
    uint64_t w_pawns_copy = pos->pieces[COLOR_IDX(WHITE)][PAWN];
    uint64_t b_pawns_copy = pos->pieces[COLOR_IDX(BLACK)][PAWN];
    uint64_t w_rooks_queens = pos->pieces[COLOR_IDX(WHITE)][ROOK] | pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    uint64_t b_rooks_queens = pos->pieces[COLOR_IDX(BLACK)][ROOK] | pos->pieces[COLOR_IDX(BLACK)][QUEEN];

    // Evaluate White pawns
    uint64_t w_pawns = w_pawns_copy;
    while (w_pawns) {
        int sq = pop_lsb(&w_pawns);
        int item_score_mg = PIECE_PAWN_VAL + pawn_table[sq];
        int item_score_eg = PIECE_PAWN_VAL + pawn_table[sq];

        // Isolated pawn evaluation
        if (!(adjacentFilesMask[sq] & w_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & b_pawns_copy);
            item_score_mg -= is_semi_open ? isolated_pawn_semi_open_mg : isolated_pawn_mg;
            item_score_eg -= is_semi_open ? isolated_pawn_semi_open_eg : isolated_pawn_eg;
        }

        // Doubled pawn evaluation
        if (fileBehindMasks[0][sq] & w_pawns_copy) {
            item_score_mg -= double_pawn_mg;
            item_score_eg -= double_pawn_eg;
        }

        // Passed pawn evaluation
        if (!(passedPawnMasks[COLOR_IDX(WHITE)][sq] & b_pawns_copy)) {
            int rank = RANK_OF(sq);
            int bp_mg = passed_pawn_mg[rank];
            int bp_eg = passed_pawn_eg[rank];

            // Defended/Protected by friendly pawn
            if (pawnAttacks[COLOR_IDX(BLACK)][sq] & w_pawns_copy) {
                bp_mg += PASSED_PAWN_DEFENDED_MG_VAL;
                bp_eg += PASSED_PAWN_DEFENDED_EG_VAL;
            }

            // Blocked by any piece
            int front_sq = sq + 8;
            if (pos->occAll & (1ULL << front_sq)) {
                bp_mg = (bp_mg * 7) / 10;
                bp_eg = (bp_eg * 7) / 10;
            }

            // Friendly rook/queen behind
            if (fileBehindMasks[COLOR_IDX(WHITE)][sq] & w_rooks_queens) {
                bp_mg += PASSED_PAWN_FRIENDLY_BEHIND_MG_VAL;
                bp_eg += PASSED_PAWN_FRIENDLY_BEHIND_EG_VAL;
            }

            // Enemy rook/queen behind
            if (fileBehindMasks[COLOR_IDX(WHITE)][sq] & b_rooks_queens) {
                bp_mg += PASSED_PAWN_ENEMY_BEHIND_MG_VAL;
                bp_eg += PASSED_PAWN_ENEMY_BEHIND_EG_VAL;
            }

            item_score_mg += bp_mg;
            item_score_eg += bp_eg;
        }
        mg_score += item_score_mg;
        eg_score += item_score_eg;
    }

    // Evaluate Black pawns (subtracted from score)
    uint64_t b_pawns = b_pawns_copy;
    while (b_pawns) {
        int sq = pop_lsb(&b_pawns);
        int item_score_mg = PIECE_PAWN_VAL + pawn_table[sq ^ 56];
        int item_score_eg = PIECE_PAWN_VAL + pawn_table[sq ^ 56];

        // Isolated pawn evaluation
        if (!(adjacentFilesMask[sq] & b_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & w_pawns_copy);
            item_score_mg -= is_semi_open ? isolated_pawn_semi_open_mg : isolated_pawn_mg;
            item_score_eg -= is_semi_open ? isolated_pawn_semi_open_eg : isolated_pawn_eg;
        }

        // Doubled pawn evaluation
        if (fileBehindMasks[1][sq] & b_pawns_copy) {
            item_score_mg -= double_pawn_mg;
            item_score_eg -= double_pawn_eg;
        }

        // Passed pawn evaluation
        if (!(passedPawnMasks[COLOR_IDX(BLACK)][sq] & w_pawns_copy)) {
            int rank = 7 - RANK_OF(sq);
            int bp_mg = passed_pawn_mg[rank];
            int bp_eg = passed_pawn_eg[rank];

            // Defended/Protected by friendly pawn
            if (pawnAttacks[COLOR_IDX(WHITE)][sq] & b_pawns_copy) {
                bp_mg += PASSED_PAWN_DEFENDED_MG_VAL;
                bp_eg += PASSED_PAWN_DEFENDED_EG_VAL;
            }

            // Blocked by any piece
            int front_sq = sq - 8;
            if (pos->occAll & (1ULL << front_sq)) {
                bp_mg = (bp_mg * 7) / 10;
                bp_eg = (bp_eg * 7) / 10;
            }

            // Friendly rook/queen behind
            if (fileBehindMasks[COLOR_IDX(BLACK)][sq] & b_rooks_queens) {
                bp_mg += PASSED_PAWN_FRIENDLY_BEHIND_MG_VAL;
                bp_eg += PASSED_PAWN_FRIENDLY_BEHIND_EG_VAL;
            }

            // Enemy rook/queen behind
            if (fileBehindMasks[COLOR_IDX(BLACK)][sq] & w_rooks_queens) {
                bp_mg += PASSED_PAWN_ENEMY_BEHIND_MG_VAL;
                bp_eg += PASSED_PAWN_ENEMY_BEHIND_EG_VAL;
            }

            item_score_mg += bp_mg;
            item_score_eg += bp_eg;
        }
        mg_score -= item_score_mg;
        eg_score -= item_score_eg;
    }

    return (mg_score * phase + eg_score * (24 - phase)) / 24;
}
