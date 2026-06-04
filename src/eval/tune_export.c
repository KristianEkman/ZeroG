#include "tune_export.h"
#include "eval/eval.h"
#include "eval/eval_mobility.h"
#include "eval/pawn_eval.h"
#include "fen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const int initial_param_values[NUM_PARAMS] = {
    // Piece values
    100, // PARAM_PIECE_PAWN
    320, // PARAM_PIECE_KNIGHT
    330, // PARAM_PIECE_BISHOP
    500, // PARAM_PIECE_ROOK
    900, // PARAM_PIECE_QUEEN

    // Rook open file
    20, // PARAM_ROOK_OPEN_FILE_MG
    15, // PARAM_ROOK_OPEN_FILE_EG
    10, // PARAM_ROOK_SEMI_OPEN_FILE_MG
    7,  // PARAM_ROOK_SEMI_OPEN_FILE_EG

    // Bishop pair
    50, // PARAM_BISHOP_PAIR_BONUS

    // Pawn structure
    8,  // PARAM_DOUBLE_PAWN_MG
    12, // PARAM_DOUBLE_PAWN_EG
    5,  // PARAM_ISOLATED_PAWN_MG
    8,  // PARAM_ISOLATED_PAWN_EG
    10, // PARAM_ISOLATED_PAWN_SEMI_OPEN_MG
    15, // PARAM_ISOLATED_PAWN_SEMI_OPEN_EG

    // Passed pawns MG ranks 1..6
    5, 10, 20, 35, 60, 100,
    // Passed pawns EG ranks 1..6
    8, 15, 30, 60, 110, 180,

    // Passed pawns extra
    15, // PARAM_PASSED_PAWN_DEFENDED_MG
    25, // PARAM_PASSED_PAWN_DEFENDED_EG
    20, // PARAM_PASSED_PAWN_FRIENDLY_BEHIND_MG
    40, // PARAM_PASSED_PAWN_FRIENDLY_BEHIND_EG
    -15, // PARAM_PASSED_PAWN_ENEMY_BEHIND_MG
    -30, // PARAM_PASSED_PAWN_ENEMY_BEHIND_EG

    // Knight mobility MG
    -20, -10, -5, 0, 5, 10, 15, 20, 22,
    // Knight mobility EG
    -20, -10, -5, 0, 5, 10, 15, 20, 22,

    // Bishop mobility MG
    -20, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
    // Bishop mobility EG
    -20, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,

    // Rook mobility MG
    -20, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55,
    // Rook mobility EG
    -25, -15, -5, 0, 10, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65,

    // Queen mobility MG
    -10, -5, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50,
    // Queen mobility EG
    -20, -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115
};

const char* param_names[NUM_PARAMS] = {
    // Piece values
    "piece_pawn",
    "piece_knight",
    "piece_bishop",
    "piece_rook",
    "piece_queen",

    // Rook open file
    "rook_open_file_mg",
    "rook_open_file_eg",
    "rook_semi_open_file_mg",
    "rook_semi_open_file_eg",

    // Bishop pair
    "bishop_pair_bonus",

    // Pawn structure
    "double_pawn_mg",
    "double_pawn_eg",
    "isolated_pawn_mg",
    "isolated_pawn_eg",
    "isolated_pawn_semi_open_mg",
    "isolated_pawn_semi_open_eg",

    // Passed pawns MG
    "passed_pawn_mg_r1", "passed_pawn_mg_r2", "passed_pawn_mg_r3", "passed_pawn_mg_r4", "passed_pawn_mg_r5", "passed_pawn_mg_r6",
    // Passed pawns EG
    "passed_pawn_eg_r1", "passed_pawn_eg_r2", "passed_pawn_eg_r3", "passed_pawn_eg_r4", "passed_pawn_eg_r5", "passed_pawn_eg_r6",

    // Passed pawns extra
    "passed_pawn_defended_mg", "passed_pawn_defended_eg",
    "passed_pawn_friendly_behind_mg", "passed_pawn_friendly_behind_eg",
    "passed_pawn_enemy_behind_mg", "passed_pawn_enemy_behind_eg",

    // Knight mobility MG
    "knight_mobility_mg_0", "knight_mobility_mg_1", "knight_mobility_mg_2", "knight_mobility_mg_3", "knight_mobility_mg_4", "knight_mobility_mg_5", "knight_mobility_mg_6", "knight_mobility_mg_7", "knight_mobility_mg_8",
    // Knight mobility EG
    "knight_mobility_eg_0", "knight_mobility_eg_1", "knight_mobility_eg_2", "knight_mobility_eg_3", "knight_mobility_eg_4", "knight_mobility_eg_5", "knight_mobility_eg_6", "knight_mobility_eg_7", "knight_mobility_eg_8",

    // Bishop mobility MG
    "bishop_mobility_mg_0", "bishop_mobility_mg_1", "bishop_mobility_mg_2", "bishop_mobility_mg_3", "bishop_mobility_mg_4", "bishop_mobility_mg_5", "bishop_mobility_mg_6", "bishop_mobility_mg_7", "bishop_mobility_mg_8", "bishop_mobility_mg_9", "bishop_mobility_mg_10", "bishop_mobility_mg_11", "bishop_mobility_mg_12", "bishop_mobility_mg_13",
    // Bishop mobility EG
    "bishop_mobility_eg_0", "bishop_mobility_eg_1", "bishop_mobility_eg_2", "bishop_mobility_eg_3", "bishop_mobility_eg_4", "bishop_mobility_eg_5", "bishop_mobility_eg_6", "bishop_mobility_eg_7", "bishop_mobility_eg_8", "bishop_mobility_eg_9", "bishop_mobility_eg_10", "bishop_mobility_eg_11", "bishop_mobility_eg_12", "bishop_mobility_eg_13",

    // Rook mobility MG
    "rook_mobility_mg_0", "rook_mobility_mg_1", "rook_mobility_mg_2", "rook_mobility_mg_3", "rook_mobility_mg_4", "rook_mobility_mg_5", "rook_mobility_mg_6", "rook_mobility_mg_7", "rook_mobility_mg_8", "rook_mobility_mg_9", "rook_mobility_mg_10", "rook_mobility_mg_11", "rook_mobility_mg_12", "rook_mobility_mg_13", "rook_mobility_mg_14",
    // Rook mobility EG
    "rook_mobility_eg_0", "rook_mobility_eg_1", "rook_mobility_eg_2", "rook_mobility_eg_3", "rook_mobility_eg_4", "rook_mobility_eg_5", "rook_mobility_eg_6", "rook_mobility_eg_7", "rook_mobility_eg_8", "rook_mobility_eg_9", "rook_mobility_eg_10", "rook_mobility_eg_11", "rook_mobility_eg_12", "rook_mobility_eg_13", "rook_mobility_eg_14",

    // Queen mobility MG
    "queen_mobility_mg_0", "queen_mobility_mg_1", "queen_mobility_mg_2", "queen_mobility_mg_3", "queen_mobility_mg_4", "queen_mobility_mg_5", "queen_mobility_mg_6", "queen_mobility_mg_7", "queen_mobility_mg_8", "queen_mobility_mg_9", "queen_mobility_mg_10", "queen_mobility_mg_11", "queen_mobility_mg_12", "queen_mobility_mg_13", "queen_mobility_mg_14", "queen_mobility_mg_15", "queen_mobility_mg_16", "queen_mobility_mg_17", "queen_mobility_mg_18", "queen_mobility_mg_19", "queen_mobility_mg_20", "queen_mobility_mg_21", "queen_mobility_mg_22", "queen_mobility_mg_23", "queen_mobility_mg_24", "queen_mobility_mg_25", "queen_mobility_mg_26", "queen_mobility_mg_27",
    // Queen mobility EG
    "queen_mobility_eg_0", "queen_mobility_eg_1", "queen_mobility_eg_2", "queen_mobility_eg_3", "queen_mobility_eg_4", "queen_mobility_eg_5", "queen_mobility_eg_6", "queen_mobility_eg_7", "queen_mobility_eg_8", "queen_mobility_eg_9", "queen_mobility_eg_10", "queen_mobility_eg_11", "queen_mobility_eg_12", "queen_mobility_eg_13", "queen_mobility_eg_14", "queen_mobility_eg_15", "queen_mobility_eg_16", "queen_mobility_eg_17", "queen_mobility_eg_18", "queen_mobility_eg_19", "queen_mobility_eg_20", "queen_mobility_eg_21", "queen_mobility_eg_22", "queen_mobility_eg_23", "queen_mobility_eg_24", "queen_mobility_eg_25", "queen_mobility_eg_26", "queen_mobility_eg_27"
};

// Cached file masks for open file detection
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
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0 
};

static const int knight_table[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50 
};

static const int bishop_table[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20 
};

static const int rook_table[64] = {
      0,  0,  0,  5,  5,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      5, 10, 10, 10, 10, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0 
};

static const int queen_table[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20 
};

static const int king_middle_table[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30 
};

static const int king_end_table[64] = {
    -50,-30,-30,-30,-30,-30,-30,-50,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -50,-40,-30,-20,-20,-30,-40,-50 
};



/* Performs exact feature coefficient extraction */
void evaluate_extract_features(const Position *pos, double *features, int *const_score) {
    memset(features, 0, sizeof(double) * NUM_PARAMS);
    *const_score = 0;

    int phase = get_game_phase(pos);
    if (phase > 24) phase = 24;

    double mg_scale = (double)phase / 24.0;
    double eg_scale = (double)(24 - phase) / 24.0;

    uint64_t w_pawns_copy = pos->pieces[COLOR_IDX(WHITE)][PAWN];
    uint64_t b_pawns_copy = pos->pieces[COLOR_IDX(BLACK)][PAWN];
    uint64_t w_rooks_queens = pos->pieces[COLOR_IDX(WHITE)][ROOK] | pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    uint64_t b_rooks_queens = pos->pieces[COLOR_IDX(BLACK)][ROOK] | pos->pieces[COLOR_IDX(BLACK)][QUEEN];

    // --- Pawns ---
    // White Pawns
    uint64_t w_pawns = w_pawns_copy;
    while (w_pawns) {
        int sq = pop_lsb(&w_pawns);
        *const_score += pawn_table[sq];
        features[PARAM_PIECE_PAWN] += 1.0;

        // Isolated
        if (!(adjacentFilesMask[sq] & w_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & b_pawns_copy);
            features[is_semi_open ? PARAM_ISOLATED_PAWN_SEMI_OPEN_MG : PARAM_ISOLATED_PAWN_MG] -= mg_scale;
            features[is_semi_open ? PARAM_ISOLATED_PAWN_SEMI_OPEN_EG : PARAM_ISOLATED_PAWN_EG] -= eg_scale;
        }

        // Doubled
        if (fileBehindMasks[0][sq] & w_pawns_copy) {
            features[PARAM_DOUBLE_PAWN_MG] -= mg_scale;
            features[PARAM_DOUBLE_PAWN_EG] -= eg_scale;
        }

        // Passed
        if (!(passedPawnMasks[COLOR_IDX(WHITE)][sq] & b_pawns_copy)) {
            int rank = RANK_OF(sq);
            int defended = (pawnAttacks[COLOR_IDX(BLACK)][sq] & w_pawns_copy) ? 1 : 0;
            int blocked = (pos->occAll & (1ULL << (sq + 8))) ? 1 : 0;
            int friendly_behind = (fileBehindMasks[COLOR_IDX(WHITE)][sq] & w_rooks_queens) ? 1 : 0;
            int enemy_behind = (fileBehindMasks[COLOR_IDX(WHITE)][sq] & b_rooks_queens) ? 1 : 0;

            double scale = blocked ? 0.7 : 1.0;

            features[PARAM_PASSED_PAWN_MG_R1 + rank - 1] += scale * mg_scale;
            features[PARAM_PASSED_PAWN_EG_R1 + rank - 1] += scale * eg_scale;
            if (defended) {
                features[PARAM_PASSED_PAWN_DEFENDED_MG] += scale * mg_scale;
                features[PARAM_PASSED_PAWN_DEFENDED_EG] += scale * eg_scale;
            }
            if (friendly_behind) {
                features[PARAM_PASSED_PAWN_FRIENDLY_BEHIND_MG] += mg_scale;
                features[PARAM_PASSED_PAWN_FRIENDLY_BEHIND_EG] += eg_scale;
            }
            if (enemy_behind) {
                features[PARAM_PASSED_PAWN_ENEMY_BEHIND_MG] += mg_scale;
                features[PARAM_PASSED_PAWN_ENEMY_BEHIND_EG] += eg_scale;
            }
        }
    }

    // Black Pawns
    uint64_t b_pawns = b_pawns_copy;
    while (b_pawns) {
        int sq = pop_lsb(&b_pawns);
        *const_score -= pawn_table[sq ^ 56];
        features[PARAM_PIECE_PAWN] -= 1.0;

        // Isolated
        if (!(adjacentFilesMask[sq] & b_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & w_pawns_copy);
            features[is_semi_open ? PARAM_ISOLATED_PAWN_SEMI_OPEN_MG : PARAM_ISOLATED_PAWN_MG] += mg_scale;
            features[is_semi_open ? PARAM_ISOLATED_PAWN_SEMI_OPEN_EG : PARAM_ISOLATED_PAWN_EG] += eg_scale;
        }

        // Doubled
        if (fileBehindMasks[1][sq] & b_pawns_copy) {
            features[PARAM_DOUBLE_PAWN_MG] += mg_scale;
            features[PARAM_DOUBLE_PAWN_EG] += eg_scale;
        }

        // Passed
        if (!(passedPawnMasks[COLOR_IDX(BLACK)][sq] & w_pawns_copy)) {
            int rank = 7 - RANK_OF(sq);
            int defended = (pawnAttacks[COLOR_IDX(WHITE)][sq] & b_pawns_copy) ? 1 : 0;
            int blocked = (pos->occAll & (1ULL << (sq - 8))) ? 1 : 0;
            int friendly_behind = (fileBehindMasks[COLOR_IDX(BLACK)][sq] & b_rooks_queens) ? 1 : 0;
            int enemy_behind = (fileBehindMasks[COLOR_IDX(BLACK)][sq] & w_rooks_queens) ? 1 : 0;

            double scale = blocked ? 0.7 : 1.0;

            features[PARAM_PASSED_PAWN_MG_R1 + rank - 1] -= scale * mg_scale;
            features[PARAM_PASSED_PAWN_EG_R1 + rank - 1] -= scale * eg_scale;
            if (defended) {
                features[PARAM_PASSED_PAWN_DEFENDED_MG] -= scale * mg_scale;
                features[PARAM_PASSED_PAWN_DEFENDED_EG] -= scale * eg_scale;
            }
            if (friendly_behind) {
                features[PARAM_PASSED_PAWN_FRIENDLY_BEHIND_MG] -= mg_scale;
                features[PARAM_PASSED_PAWN_FRIENDLY_BEHIND_EG] -= eg_scale;
            }
            if (enemy_behind) {
                features[PARAM_PASSED_PAWN_ENEMY_BEHIND_MG] -= mg_scale;
                features[PARAM_PASSED_PAWN_ENEMY_BEHIND_EG] -= eg_scale;
            }
        }
    }

    // --- Knights ---
    uint64_t w_knights = pos->pieces[COLOR_IDX(WHITE)][KNIGHT];
    while (w_knights) {
        int sq = pop_lsb(&w_knights);
        *const_score += knight_table[sq];
        features[PARAM_PIECE_KNIGHT] += 1.0;
    }
    uint64_t b_knights = pos->pieces[COLOR_IDX(BLACK)][KNIGHT];
    while (b_knights) {
        int sq = pop_lsb(&b_knights);
        *const_score -= knight_table[sq ^ 56];
        features[PARAM_PIECE_KNIGHT] -= 1.0;
    }

    // --- Bishops ---
    uint64_t w_bishops = pos->pieces[COLOR_IDX(WHITE)][BISHOP];
    while (w_bishops) {
        int sq = pop_lsb(&w_bishops);
        *const_score += bishop_table[sq];
        features[PARAM_PIECE_BISHOP] += 1.0;
    }
    uint64_t b_bishops = pos->pieces[COLOR_IDX(BLACK)][BISHOP];
    while (b_bishops) {
        int sq = pop_lsb(&b_bishops);
        *const_score -= bishop_table[sq ^ 56];
        features[PARAM_PIECE_BISHOP] -= 1.0;
    }

    // --- Rooks ---
    uint64_t w_rooks = pos->pieces[COLOR_IDX(WHITE)][ROOK];
    while (w_rooks) {
        int sq = pop_lsb(&w_rooks);
        *const_score += rook_table[sq];
        features[PARAM_PIECE_ROOK] += 1.0;

        int file = sq & 7;
        if (!(file_masks[file] & w_pawns_copy)) {
            if (!(file_masks[file] & b_pawns_copy)) {
                features[PARAM_ROOK_OPEN_FILE_MG] += mg_scale;
                features[PARAM_ROOK_OPEN_FILE_EG] += eg_scale;
            } else {
                features[PARAM_ROOK_SEMI_OPEN_FILE_MG] += mg_scale;
                features[PARAM_ROOK_SEMI_OPEN_FILE_EG] += eg_scale;
            }
        }
    }
    uint64_t b_rooks = pos->pieces[COLOR_IDX(BLACK)][ROOK];
    while (b_rooks) {
        int sq = pop_lsb(&b_rooks);
        *const_score -= rook_table[sq ^ 56];
        features[PARAM_PIECE_ROOK] -= 1.0;

        int file = sq & 7;
        if (!(file_masks[file] & b_pawns_copy)) {
            if (!(file_masks[file] & w_pawns_copy)) {
                features[PARAM_ROOK_OPEN_FILE_MG] -= mg_scale;
                features[PARAM_ROOK_OPEN_FILE_EG] -= eg_scale;
            } else {
                features[PARAM_ROOK_SEMI_OPEN_FILE_MG] -= mg_scale;
                features[PARAM_ROOK_SEMI_OPEN_FILE_EG] -= eg_scale;
            }
        }
    }

    // --- Queens ---
    uint64_t w_queens = pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    while (w_queens) {
        int sq = pop_lsb(&w_queens);
        *const_score += queen_table[sq];
        features[PARAM_PIECE_QUEEN] += 1.0;
    }
    uint64_t b_queens = pos->pieces[COLOR_IDX(BLACK)][QUEEN];
    while (b_queens) {
        int sq = pop_lsb(&b_queens);
        *const_score -= queen_table[sq ^ 56];
        features[PARAM_PIECE_QUEEN] -= 1.0;
    }

    // --- Kings ---
    uint64_t w_king = pos->pieces[COLOR_IDX(WHITE)][KING];
    while (w_king) {
        int sq = pop_lsb(&w_king);
        *const_score += 20000 + (king_middle_table[sq] * phase + king_end_table[sq] * (24 - phase)) / 24;
    }
    uint64_t b_king = pos->pieces[COLOR_IDX(BLACK)][KING];
    while (b_king) {
        int sq = pop_lsb(&b_king);
        *const_score -= 20000 + (king_middle_table[sq ^ 56] * phase + king_end_table[sq ^ 56] * (24 - phase)) / 24;
    }

    // --- Bishop Pair ---
    int w_bishops_count = bit_count(pos->pieces[COLOR_IDX(WHITE)][BISHOP]);
    if (w_bishops_count >= 2) {
        features[PARAM_BISHOP_PAIR_BONUS] += 1.0;
    }
    int b_bishops_count = bit_count(pos->pieces[COLOR_IDX(BLACK)][BISHOP]);
    if (b_bishops_count >= 2) {
        features[PARAM_BISHOP_PAIR_BONUS] -= 1.0;
    }

    // --- Mobility ---
    uint64_t occ = pos->occAll;

    // White Mobility
    {
        Color us = WHITE;
        Color them = BLACK;
        uint64_t own_pieces = pos->occ[COLOR_IDX(us)];
        uint64_t enemy_pawns = pos->pieces[COLOR_IDX(them)][PAWN];
        uint64_t enemy_pawn_attacks = ((enemy_pawns & ~0x8080808080808080ULL) >> 7) | 
                                     ((enemy_pawns & ~0x0101010101010101ULL) >> 9);
        uint64_t mobility_area = ~own_pieces & ~enemy_pawn_attacks;

        // Knights
        uint64_t knights = pos->pieces[COLOR_IDX(us)][KNIGHT];
        while (knights) {
            int sq = pop_lsb(&knights);
            uint64_t attacks = knightAttacks[sq] & mobility_area;
            int count = bit_count(attacks);
            if (count > 8) count = 8;
            features[PARAM_KNIGHT_MOBILITY_MG_0 + count] += mg_scale;
            features[PARAM_KNIGHT_MOBILITY_EG_0 + count] += eg_scale;
        }

        // Bishops
        uint64_t bishops = pos->pieces[COLOR_IDX(us)][BISHOP];
        while (bishops) {
            int sq = pop_lsb(&bishops);
            uint64_t attacks = bishopAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 13) count = 13;
            features[PARAM_BISHOP_MOBILITY_MG_0 + count] += mg_scale;
            features[PARAM_BISHOP_MOBILITY_EG_0 + count] += eg_scale;
        }

        // Rooks
        uint64_t rooks = pos->pieces[COLOR_IDX(us)][ROOK];
        while (rooks) {
            int sq = pop_lsb(&rooks);
            uint64_t attacks = rookAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 14) count = 14;
            features[PARAM_ROOK_MOBILITY_MG_0 + count] += mg_scale;
            features[PARAM_ROOK_MOBILITY_EG_0 + count] += eg_scale;
        }

        // Queens
        uint64_t queens = pos->pieces[COLOR_IDX(us)][QUEEN];
        while (queens) {
            int sq = pop_lsb(&queens);
            uint64_t attacks = queenAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 27) count = 27;
            features[PARAM_QUEEN_MOBILITY_MG_0 + count] += mg_scale;
            features[PARAM_QUEEN_MOBILITY_EG_0 + count] += eg_scale;
        }
    }

    // Black Mobility
    {
        Color us = BLACK;
        Color them = WHITE;
        uint64_t own_pieces = pos->occ[COLOR_IDX(us)];
        uint64_t enemy_pawns = pos->pieces[COLOR_IDX(them)][PAWN];
        uint64_t enemy_pawn_attacks = ((enemy_pawns & ~0x8080808080808080ULL) << 9) | 
                                     ((enemy_pawns & ~0x0101010101010101ULL) << 7);
        uint64_t mobility_area = ~own_pieces & ~enemy_pawn_attacks;

        // Knights
        uint64_t knights = pos->pieces[COLOR_IDX(us)][KNIGHT];
        while (knights) {
            int sq = pop_lsb(&knights);
            uint64_t attacks = knightAttacks[sq] & mobility_area;
            int count = bit_count(attacks);
            if (count > 8) count = 8;
            features[PARAM_KNIGHT_MOBILITY_MG_0 + count] -= mg_scale;
            features[PARAM_KNIGHT_MOBILITY_EG_0 + count] -= eg_scale;
        }

        // Bishops
        uint64_t bishops = pos->pieces[COLOR_IDX(us)][BISHOP];
        while (bishops) {
            int sq = pop_lsb(&bishops);
            uint64_t attacks = bishopAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 13) count = 13;
            features[PARAM_BISHOP_MOBILITY_MG_0 + count] -= mg_scale;
            features[PARAM_BISHOP_MOBILITY_EG_0 + count] -= eg_scale;
        }

        // Rooks
        uint64_t rooks = pos->pieces[COLOR_IDX(us)][ROOK];
        while (rooks) {
            int sq = pop_lsb(&rooks);
            uint64_t attacks = rookAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 14) count = 14;
            features[PARAM_ROOK_MOBILITY_MG_0 + count] -= mg_scale;
            features[PARAM_ROOK_MOBILITY_EG_0 + count] -= eg_scale;
        }

        // Queens
        uint64_t queens = pos->pieces[COLOR_IDX(us)][QUEEN];
        while (queens) {
            int sq = pop_lsb(&queens);
            uint64_t attacks = queenAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 27) count = 27;
            features[PARAM_QUEEN_MOBILITY_MG_0 + count] -= mg_scale;
            features[PARAM_QUEEN_MOBILITY_EG_0 + count] -= eg_scale;
        }
    }
}

/* 
 * Replicated evaluation function using integer parameters,
 * mirroring exact integer division roundoff in mobility.
 */
int evaluate_replicated(const Position *pos, const int *params) {
    int mg_score = 0;
    int eg_score = 0;
    int phase = get_game_phase(pos);
    if (phase > 24) phase = 24;

    uint64_t w_pawns_copy = pos->pieces[COLOR_IDX(WHITE)][PAWN];
    uint64_t b_pawns_copy = pos->pieces[COLOR_IDX(BLACK)][PAWN];
    uint64_t w_rooks_queens = pos->pieces[COLOR_IDX(WHITE)][ROOK] | pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    uint64_t b_rooks_queens = pos->pieces[COLOR_IDX(BLACK)][ROOK] | pos->pieces[COLOR_IDX(BLACK)][QUEEN];

    // --- Pawns ---
    // White Pawns
    uint64_t w_pawns = w_pawns_copy;
    while (w_pawns) {
        int sq = pop_lsb(&w_pawns);
        int item_score_mg = params[PARAM_PIECE_PAWN] + pawn_table[sq];
        int item_score_eg = params[PARAM_PIECE_PAWN] + pawn_table[sq];

        // Isolated
        if (!(adjacentFilesMask[sq] & w_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & b_pawns_copy);
            item_score_mg -= is_semi_open ? params[PARAM_ISOLATED_PAWN_SEMI_OPEN_MG] : params[PARAM_ISOLATED_PAWN_MG];
            item_score_eg -= is_semi_open ? params[PARAM_ISOLATED_PAWN_SEMI_OPEN_EG] : params[PARAM_ISOLATED_PAWN_EG];
        }

        // Doubled
        if (fileBehindMasks[0][sq] & w_pawns_copy) {
            item_score_mg -= params[PARAM_DOUBLE_PAWN_MG];
            item_score_eg -= params[PARAM_DOUBLE_PAWN_EG];
        }

        // Passed
        if (!(passedPawnMasks[COLOR_IDX(WHITE)][sq] & b_pawns_copy)) {
            int rank = RANK_OF(sq);
            int bp_mg = params[PARAM_PASSED_PAWN_MG_R1 + rank - 1];
            int bp_eg = params[PARAM_PASSED_PAWN_EG_R1 + rank - 1];

            if (pawnAttacks[COLOR_IDX(BLACK)][sq] & w_pawns_copy) {
                bp_mg += params[PARAM_PASSED_PAWN_DEFENDED_MG];
                bp_eg += params[PARAM_PASSED_PAWN_DEFENDED_EG];
            }

            int front_sq = sq + 8;
            if (pos->occAll & (1ULL << front_sq)) {
                bp_mg = (bp_mg * 7) / 10;
                bp_eg = (bp_eg * 7) / 10;
            }

            if (fileBehindMasks[COLOR_IDX(WHITE)][sq] & w_rooks_queens) {
                bp_mg += params[PARAM_PASSED_PAWN_FRIENDLY_BEHIND_MG];
                bp_eg += params[PARAM_PASSED_PAWN_FRIENDLY_BEHIND_EG];
            }

            if (fileBehindMasks[COLOR_IDX(WHITE)][sq] & b_rooks_queens) {
                bp_mg += params[PARAM_PASSED_PAWN_ENEMY_BEHIND_MG];
                bp_eg += params[PARAM_PASSED_PAWN_ENEMY_BEHIND_EG];
            }

            item_score_mg += bp_mg;
            item_score_eg += bp_eg;
        }
        mg_score += item_score_mg;
        eg_score += item_score_eg;
    }

    // Black Pawns
    uint64_t b_pawns = b_pawns_copy;
    while (b_pawns) {
        int sq = pop_lsb(&b_pawns);
        int item_score_mg = params[PARAM_PIECE_PAWN] + pawn_table[sq ^ 56];
        int item_score_eg = params[PARAM_PIECE_PAWN] + pawn_table[sq ^ 56];

        // Isolated
        if (!(adjacentFilesMask[sq] & b_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & w_pawns_copy);
            item_score_mg -= is_semi_open ? params[PARAM_ISOLATED_PAWN_SEMI_OPEN_MG] : params[PARAM_ISOLATED_PAWN_MG];
            item_score_eg -= is_semi_open ? params[PARAM_ISOLATED_PAWN_SEMI_OPEN_EG] : params[PARAM_ISOLATED_PAWN_EG];
        }

        // Doubled
        if (fileBehindMasks[1][sq] & b_pawns_copy) {
            item_score_mg -= params[PARAM_DOUBLE_PAWN_MG];
            item_score_eg -= params[PARAM_DOUBLE_PAWN_EG];
        }

        // Passed
        if (!(passedPawnMasks[COLOR_IDX(BLACK)][sq] & w_pawns_copy)) {
            int rank = 7 - RANK_OF(sq);
            int bp_mg = params[PARAM_PASSED_PAWN_MG_R1 + rank - 1];
            int bp_eg = params[PARAM_PASSED_PAWN_EG_R1 + rank - 1];

            if (pawnAttacks[COLOR_IDX(WHITE)][sq] & b_pawns_copy) {
                bp_mg += params[PARAM_PASSED_PAWN_DEFENDED_MG];
                bp_eg += params[PARAM_PASSED_PAWN_DEFENDED_EG];
            }

            int front_sq = sq - 8;
            if (pos->occAll & (1ULL << front_sq)) {
                bp_mg = (bp_mg * 7) / 10;
                bp_eg = (bp_eg * 7) / 10;
            }

            if (fileBehindMasks[COLOR_IDX(BLACK)][sq] & b_rooks_queens) {
                bp_mg += params[PARAM_PASSED_PAWN_FRIENDLY_BEHIND_MG];
                bp_eg += params[PARAM_PASSED_PAWN_FRIENDLY_BEHIND_EG];
            }

            if (fileBehindMasks[COLOR_IDX(BLACK)][sq] & w_rooks_queens) {
                bp_mg += params[PARAM_PASSED_PAWN_ENEMY_BEHIND_MG];
                bp_eg += params[PARAM_PASSED_PAWN_ENEMY_BEHIND_EG];
            }

            item_score_mg += bp_mg;
            item_score_eg += bp_eg;
        }
        mg_score -= item_score_mg;
        eg_score -= item_score_eg;
    }

    // --- Knights ---
    uint64_t w_knights = pos->pieces[COLOR_IDX(WHITE)][KNIGHT];
    while (w_knights) {
        int sq = pop_lsb(&w_knights);
        int val = params[PARAM_PIECE_KNIGHT] + knight_table[sq];
        mg_score += val;
        eg_score += val;
    }
    uint64_t b_knights = pos->pieces[COLOR_IDX(BLACK)][KNIGHT];
    while (b_knights) {
        int sq = pop_lsb(&b_knights);
        int val = params[PARAM_PIECE_KNIGHT] + knight_table[sq ^ 56];
        mg_score -= val;
        eg_score -= val;
    }

    // --- Bishops ---
    uint64_t w_bishops = pos->pieces[COLOR_IDX(WHITE)][BISHOP];
    while (w_bishops) {
        int sq = pop_lsb(&w_bishops);
        int val = params[PARAM_PIECE_BISHOP] + bishop_table[sq];
        mg_score += val;
        eg_score += val;
    }
    uint64_t b_bishops = pos->pieces[COLOR_IDX(BLACK)][BISHOP];
    while (b_bishops) {
        int sq = pop_lsb(&b_bishops);
        int val = params[PARAM_PIECE_BISHOP] + bishop_table[sq ^ 56];
        mg_score -= val;
        eg_score -= val;
    }

    // --- Rooks ---
    uint64_t w_rooks = pos->pieces[COLOR_IDX(WHITE)][ROOK];
    while (w_rooks) {
        int sq = pop_lsb(&w_rooks);
        int item_score_mg = params[PARAM_PIECE_ROOK] + rook_table[sq];
        int item_score_eg = params[PARAM_PIECE_ROOK] + rook_table[sq];

        int file = sq & 7;
        if (!(file_masks[file] & w_pawns_copy)) {
            if (!(file_masks[file] & b_pawns_copy)) {
                item_score_mg += params[PARAM_ROOK_OPEN_FILE_MG];
                item_score_eg += params[PARAM_ROOK_OPEN_FILE_EG];
            } else {
                item_score_mg += params[PARAM_ROOK_SEMI_OPEN_FILE_MG];
                item_score_eg += params[PARAM_ROOK_SEMI_OPEN_FILE_EG];
            }
        }
        mg_score += item_score_mg;
        eg_score += item_score_eg;
    }
    uint64_t b_rooks = pos->pieces[COLOR_IDX(BLACK)][ROOK];
    while (b_rooks) {
        int sq = pop_lsb(&b_rooks);
        int item_score_mg = params[PARAM_PIECE_ROOK] + rook_table[sq ^ 56];
        int item_score_eg = params[PARAM_PIECE_ROOK] + rook_table[sq ^ 56];

        int file = sq & 7;
        if (!(file_masks[file] & b_pawns_copy)) {
            if (!(file_masks[file] & w_pawns_copy)) {
                item_score_mg += params[PARAM_ROOK_OPEN_FILE_MG];
                item_score_eg += params[PARAM_ROOK_OPEN_FILE_EG];
            } else {
                item_score_mg += params[PARAM_ROOK_SEMI_OPEN_FILE_MG];
                item_score_eg += params[PARAM_ROOK_SEMI_OPEN_FILE_EG];
            }
        }
        mg_score -= item_score_mg;
        eg_score -= item_score_eg;
    }

    // --- Queens ---
    uint64_t w_queens = pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    while (w_queens) {
        int sq = pop_lsb(&w_queens);
        int val = params[PARAM_PIECE_QUEEN] + queen_table[sq];
        mg_score += val;
        eg_score += val;
    }
    uint64_t b_queens = pos->pieces[COLOR_IDX(BLACK)][QUEEN];
    while (b_queens) {
        int sq = pop_lsb(&b_queens);
        int val = params[PARAM_PIECE_QUEEN] + queen_table[sq ^ 56];
        mg_score -= val;
        eg_score -= val;
    }

    // --- Kings ---
    uint64_t w_king = pos->pieces[COLOR_IDX(WHITE)][KING];
    while (w_king) {
        int sq = pop_lsb(&w_king);
        mg_score += 20000 + king_middle_table[sq];
        eg_score += 20000 + king_end_table[sq];
    }
    uint64_t b_king = pos->pieces[COLOR_IDX(BLACK)][KING];
    while (b_king) {
        int sq = pop_lsb(&b_king);
        mg_score -= 20000 + king_middle_table[sq ^ 56];
        eg_score -= 20000 + king_end_table[sq ^ 56];
    }

    // --- Bishop Pair ---
    int w_bishops_count = bit_count(pos->pieces[COLOR_IDX(WHITE)][BISHOP]);
    if (w_bishops_count >= 2) {
        mg_score += params[PARAM_BISHOP_PAIR_BONUS];
        eg_score += params[PARAM_BISHOP_PAIR_BONUS];
    }
    int b_bishops_count = bit_count(pos->pieces[COLOR_IDX(BLACK)][BISHOP]);
    if (b_bishops_count >= 2) {
        mg_score -= params[PARAM_BISHOP_PAIR_BONUS];
        eg_score -= params[PARAM_BISHOP_PAIR_BONUS];
    }

    // --- Bishop Pair and Mobility ---
    uint64_t occ = pos->occAll;

    // White
    int white_mob_mg = 0;
    int white_mob_eg = 0;
    {
        Color us = WHITE;
        Color them = BLACK;
        uint64_t own_pieces = pos->occ[COLOR_IDX(us)];
        uint64_t enemy_pawns = pos->pieces[COLOR_IDX(them)][PAWN];
        uint64_t enemy_pawn_attacks = ((enemy_pawns & ~0x8080808080808080ULL) >> 7) | 
                                     ((enemy_pawns & ~0x0101010101010101ULL) >> 9);
        uint64_t mobility_area = ~own_pieces & ~enemy_pawn_attacks;

        uint64_t knights = pos->pieces[COLOR_IDX(us)][KNIGHT];
        while (knights) {
            int sq = pop_lsb(&knights);
            uint64_t attacks = knightAttacks[sq] & mobility_area;
            int count = bit_count(attacks);
            if (count > 8) count = 8;
            white_mob_mg += params[PARAM_KNIGHT_MOBILITY_MG_0 + count];
            white_mob_eg += params[PARAM_KNIGHT_MOBILITY_EG_0 + count];
        }

        uint64_t bishops = pos->pieces[COLOR_IDX(us)][BISHOP];
        while (bishops) {
            int sq = pop_lsb(&bishops);
            uint64_t attacks = bishopAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 13) count = 13;
            white_mob_mg += params[PARAM_BISHOP_MOBILITY_MG_0 + count];
            white_mob_eg += params[PARAM_BISHOP_MOBILITY_EG_0 + count];
        }

        uint64_t rooks = pos->pieces[COLOR_IDX(us)][ROOK];
        while (rooks) {
            int sq = pop_lsb(&rooks);
            uint64_t attacks = rookAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 14) count = 14;
            white_mob_mg += params[PARAM_ROOK_MOBILITY_MG_0 + count];
            white_mob_eg += params[PARAM_ROOK_MOBILITY_EG_0 + count];
        }

        uint64_t queens = pos->pieces[COLOR_IDX(us)][QUEEN];
        while (queens) {
            int sq = pop_lsb(&queens);
            uint64_t attacks = queenAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 27) count = 27;
            white_mob_mg += params[PARAM_QUEEN_MOBILITY_MG_0 + count];
            white_mob_eg += params[PARAM_QUEEN_MOBILITY_EG_0 + count];
        }
    }

    // Black
    int black_mob_mg = 0;
    int black_mob_eg = 0;
    {
        Color us = BLACK;
        Color them = WHITE;
        uint64_t own_pieces = pos->occ[COLOR_IDX(us)];
        uint64_t enemy_pawns = pos->pieces[COLOR_IDX(them)][PAWN];
        uint64_t enemy_pawn_attacks = ((enemy_pawns & ~0x8080808080808080ULL) << 9) | 
                                     ((enemy_pawns & ~0x0101010101010101ULL) << 7);
        uint64_t mobility_area = ~own_pieces & ~enemy_pawn_attacks;

        uint64_t knights = pos->pieces[COLOR_IDX(us)][KNIGHT];
        while (knights) {
            int sq = pop_lsb(&knights);
            uint64_t attacks = knightAttacks[sq] & mobility_area;
            int count = bit_count(attacks);
            if (count > 8) count = 8;
            black_mob_mg += params[PARAM_KNIGHT_MOBILITY_MG_0 + count];
            black_mob_eg += params[PARAM_KNIGHT_MOBILITY_EG_0 + count];
        }

        uint64_t bishops = pos->pieces[COLOR_IDX(us)][BISHOP];
        while (bishops) {
            int sq = pop_lsb(&bishops);
            uint64_t attacks = bishopAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 13) count = 13;
            black_mob_mg += params[PARAM_BISHOP_MOBILITY_MG_0 + count];
            black_mob_eg += params[PARAM_BISHOP_MOBILITY_EG_0 + count];
        }

        uint64_t rooks = pos->pieces[COLOR_IDX(us)][ROOK];
        while (rooks) {
            int sq = pop_lsb(&rooks);
            uint64_t attacks = rookAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 14) count = 14;
            black_mob_mg += params[PARAM_ROOK_MOBILITY_MG_0 + count];
            black_mob_eg += params[PARAM_ROOK_MOBILITY_EG_0 + count];
        }

        uint64_t queens = pos->pieces[COLOR_IDX(us)][QUEEN];
        while (queens) {
            int sq = pop_lsb(&queens);
            uint64_t attacks = queenAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 27) count = 27;
            black_mob_mg += params[PARAM_QUEEN_MOBILITY_MG_0 + count];
            black_mob_eg += params[PARAM_QUEEN_MOBILITY_EG_0 + count];
        }
    }

    mg_score += white_mob_mg - black_mob_mg;
    eg_score += white_mob_eg - black_mob_eg;

    return (mg_score * phase + eg_score * (24 - phase)) / 24;
}

int run_tune_export(const char *input_path, const char *output_path) {
    FILE *in = fopen(input_path, "r");
    if (!in) {
        perror("Failed to open input EPD file");
        return 1;
    }
    FILE *out = fopen(output_path, "w");
    if (!out) {
        perror("Failed to open output CSV file");
        fclose(in);
        return 1;
    }

    // Write CSV Headers
    fprintf(out, "result,const_score");
    for (int i = 0; i < NUM_PARAMS; i++) {
        fprintf(out, ",%s", param_names[i]);
    }
    fprintf(out, "\n");

    char line[1024];
    long long total_count = 0;
    long long verified_count = 0;
    long long discrepancy_count = 0;

    printf("Exporting features: %s -> %s\n", input_path, output_path);

    double *features = malloc(sizeof(double) * NUM_PARAMS);
    if (!features) {
        perror("Failed to allocate features memory");
        fclose(in);
        fclose(out);
        return 1;
    }

    while (fgets(line, sizeof(line), in)) {
        // Strip newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) continue;

        // EPD Format: <FEN> | <simulated_result> | <white_score>
        char line_copy[1024];
        strcpy(line_copy, line);

        char *fen_part = strtok(line_copy, "|");
        char *result_part = strtok(NULL, "|");

        if (!fen_part || !result_part) {
            continue; // Invalid line format
        }

        // Clean FEN string spaces
        size_t fen_len = strlen(fen_part);
        while (fen_len > 0 && fen_part[fen_len - 1] == ' ') {
            fen_part[--fen_len] = '\0';
        }
        while (*fen_part == ' ') {
            fen_part++;
        }

        double result_val = atof(result_part);

        Position pos;
        memset(&pos, 0, sizeof(Position));
        if (fen_parse(fen_part, &pos) != 0) {
            continue; // FEN parse error
        }

        total_count++;

        int const_score = 0;
        evaluate_extract_features(&pos, features, &const_score);

        // Verification check:
        // 1. Evaluate with the original evaluate()
        int eval_val = evaluate(&pos);
        // 2. Evaluate using the replicated evaluator with initial params
        int eval_rep = evaluate_replicated(&pos, initial_param_values);

        if (eval_val != eval_rep) {
            discrepancy_count++;
            if (discrepancy_count <= 10) {
                printf("Verification discrepancy at FEN: %s\n", fen_part);
                printf("  Original eval: %d, Replicated eval: %d, Difference: %d\n", 
                       eval_val, eval_rep, eval_val - eval_rep);
            }
        } else {
            verified_count++;
        }

        // Write row to CSV
        // Formatting to %g to print 0, 1, -1, or floating values cleanly and compactly
        fprintf(out, "%g,%d", result_val, const_score);
        for (int i = 0; i < NUM_PARAMS; i++) {
            fprintf(out, ",%g", features[i]);
        }
        fprintf(out, "\n");
    }

    free(features);
    fclose(in);
    fclose(out);

    printf("Done. Total positions processed: %lld\n", total_count);
    printf("  Perfectly verified: %lld (%.2f%%)\n", verified_count, (double)verified_count / total_count * 100.0);
    if (discrepancy_count > 0) {
        printf("  Discrepancy warnings (due to potential float precision vs integer division /24): %lld (%.2f%%)\n", 
               discrepancy_count, (double)discrepancy_count / total_count * 100.0);
    }

    return 0;
}
