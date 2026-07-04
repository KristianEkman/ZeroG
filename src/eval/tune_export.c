#include "tune_export.h"
#include "eval/eval.h"
#include "eval/eval_mobility.h"
#include "eval/pawn_eval.h"
#include "eval/eval_constants.h"
#include "eval/king_safety.h"
#include "fen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const int initial_param_values[NUM_PARAMS] = {
    // Piece values
    PIECE_PAWN_VAL,    // PARAM_PIECE_PAWN
    PIECE_KNIGHT_VAL,  // PARAM_PIECE_KNIGHT
    PIECE_BISHOP_VAL,  // PARAM_PIECE_BISHOP
    PIECE_ROOK_VAL,    // PARAM_PIECE_ROOK
    PIECE_QUEEN_VAL,   // PARAM_PIECE_QUEEN

    // Rook open file
    ROOK_OPEN_FILE_MG_VAL,
    ROOK_OPEN_FILE_EG_VAL,
    ROOK_SEMI_OPEN_FILE_MG_VAL,
    ROOK_SEMI_OPEN_FILE_EG_VAL,

    // Bishop pair
    BISHOP_PAIR_BONUS_VAL,

    // Pawn structure
    DOUBLE_PAWN_MG_VAL, DOUBLE_PAWN_EG_VAL,
    ISOLATED_PAWN_MG_VAL, ISOLATED_PAWN_EG_VAL,
    ISOLATED_PAWN_SEMI_OPEN_MG_VAL, ISOLATED_PAWN_SEMI_OPEN_EG_VAL,

    // Passed pawns MG ranks 1..6
    PASSED_PAWN_MG_R1_VAL, PASSED_PAWN_MG_R2_VAL, PASSED_PAWN_MG_R3_VAL,
    PASSED_PAWN_MG_R4_VAL, PASSED_PAWN_MG_R5_VAL, PASSED_PAWN_MG_R6_VAL,
    // Passed pawns EG ranks 1..6
    PASSED_PAWN_EG_R1_VAL, PASSED_PAWN_EG_R2_VAL, PASSED_PAWN_EG_R3_VAL,
    PASSED_PAWN_EG_R4_VAL, PASSED_PAWN_EG_R5_VAL, PASSED_PAWN_EG_R6_VAL,

    // Passed pawns extra
    PASSED_PAWN_DEFENDED_MG_VAL, PASSED_PAWN_DEFENDED_EG_VAL,
    PASSED_PAWN_FRIENDLY_BEHIND_MG_VAL, PASSED_PAWN_FRIENDLY_BEHIND_EG_VAL,
    PASSED_PAWN_ENEMY_BEHIND_MG_VAL, PASSED_PAWN_ENEMY_BEHIND_EG_VAL,

    // Knight mobility MG
    KNIGHT_MOBILITY_MG_0_VAL, KNIGHT_MOBILITY_MG_1_VAL, KNIGHT_MOBILITY_MG_2_VAL,
    KNIGHT_MOBILITY_MG_3_VAL, KNIGHT_MOBILITY_MG_4_VAL, KNIGHT_MOBILITY_MG_5_VAL,
    KNIGHT_MOBILITY_MG_6_VAL, KNIGHT_MOBILITY_MG_7_VAL, KNIGHT_MOBILITY_MG_8_VAL,
    // Knight mobility EG
    KNIGHT_MOBILITY_EG_0_VAL, KNIGHT_MOBILITY_EG_1_VAL, KNIGHT_MOBILITY_EG_2_VAL,
    KNIGHT_MOBILITY_EG_3_VAL, KNIGHT_MOBILITY_EG_4_VAL, KNIGHT_MOBILITY_EG_5_VAL,
    KNIGHT_MOBILITY_EG_6_VAL, KNIGHT_MOBILITY_EG_7_VAL, KNIGHT_MOBILITY_EG_8_VAL,

    // Bishop mobility MG
    BISHOP_MOBILITY_MG_0_VAL, BISHOP_MOBILITY_MG_1_VAL, BISHOP_MOBILITY_MG_2_VAL,
    BISHOP_MOBILITY_MG_3_VAL, BISHOP_MOBILITY_MG_4_VAL, BISHOP_MOBILITY_MG_5_VAL,
    BISHOP_MOBILITY_MG_6_VAL, BISHOP_MOBILITY_MG_7_VAL, BISHOP_MOBILITY_MG_8_VAL,
    BISHOP_MOBILITY_MG_9_VAL, BISHOP_MOBILITY_MG_10_VAL, BISHOP_MOBILITY_MG_11_VAL,
    BISHOP_MOBILITY_MG_12_VAL, BISHOP_MOBILITY_MG_13_VAL,
    // Bishop mobility EG
    BISHOP_MOBILITY_EG_0_VAL, BISHOP_MOBILITY_EG_1_VAL, BISHOP_MOBILITY_EG_2_VAL,
    BISHOP_MOBILITY_EG_3_VAL, BISHOP_MOBILITY_EG_4_VAL, BISHOP_MOBILITY_EG_5_VAL,
    BISHOP_MOBILITY_EG_6_VAL, BISHOP_MOBILITY_EG_7_VAL, BISHOP_MOBILITY_EG_8_VAL,
    BISHOP_MOBILITY_EG_9_VAL, BISHOP_MOBILITY_EG_10_VAL, BISHOP_MOBILITY_EG_11_VAL,
    BISHOP_MOBILITY_EG_12_VAL, BISHOP_MOBILITY_EG_13_VAL,

    // Rook mobility MG
    ROOK_MOBILITY_MG_0_VAL, ROOK_MOBILITY_MG_1_VAL, ROOK_MOBILITY_MG_2_VAL,
    ROOK_MOBILITY_MG_3_VAL, ROOK_MOBILITY_MG_4_VAL, ROOK_MOBILITY_MG_5_VAL,
    ROOK_MOBILITY_MG_6_VAL, ROOK_MOBILITY_MG_7_VAL, ROOK_MOBILITY_MG_8_VAL,
    ROOK_MOBILITY_MG_9_VAL, ROOK_MOBILITY_MG_10_VAL, ROOK_MOBILITY_MG_11_VAL,
    ROOK_MOBILITY_MG_12_VAL, ROOK_MOBILITY_MG_13_VAL, ROOK_MOBILITY_MG_14_VAL,
    // Rook mobility EG
    ROOK_MOBILITY_EG_0_VAL, ROOK_MOBILITY_EG_1_VAL, ROOK_MOBILITY_EG_2_VAL,
    ROOK_MOBILITY_EG_3_VAL, ROOK_MOBILITY_EG_4_VAL, ROOK_MOBILITY_EG_5_VAL,
    ROOK_MOBILITY_EG_6_VAL, ROOK_MOBILITY_EG_7_VAL, ROOK_MOBILITY_EG_8_VAL,
    ROOK_MOBILITY_EG_9_VAL, ROOK_MOBILITY_EG_10_VAL, ROOK_MOBILITY_EG_11_VAL,
    ROOK_MOBILITY_EG_12_VAL, ROOK_MOBILITY_EG_13_VAL, ROOK_MOBILITY_EG_14_VAL,

    // Queen mobility MG
    QUEEN_MOBILITY_MG_0_VAL, QUEEN_MOBILITY_MG_1_VAL, QUEEN_MOBILITY_MG_2_VAL,
    QUEEN_MOBILITY_MG_3_VAL, QUEEN_MOBILITY_MG_4_VAL, QUEEN_MOBILITY_MG_5_VAL,
    QUEEN_MOBILITY_MG_6_VAL, QUEEN_MOBILITY_MG_7_VAL, QUEEN_MOBILITY_MG_8_VAL,
    QUEEN_MOBILITY_MG_9_VAL, QUEEN_MOBILITY_MG_10_VAL, QUEEN_MOBILITY_MG_11_VAL,
    QUEEN_MOBILITY_MG_12_VAL, QUEEN_MOBILITY_MG_13_VAL, QUEEN_MOBILITY_MG_14_VAL,
    QUEEN_MOBILITY_MG_15_VAL, QUEEN_MOBILITY_MG_16_VAL, QUEEN_MOBILITY_MG_17_VAL,
    QUEEN_MOBILITY_MG_18_VAL, QUEEN_MOBILITY_MG_19_VAL, QUEEN_MOBILITY_MG_20_VAL,
    QUEEN_MOBILITY_MG_21_VAL, QUEEN_MOBILITY_MG_22_VAL, QUEEN_MOBILITY_MG_23_VAL,
    QUEEN_MOBILITY_MG_24_VAL, QUEEN_MOBILITY_MG_25_VAL, QUEEN_MOBILITY_MG_26_VAL,
    QUEEN_MOBILITY_MG_27_VAL,
    // Queen mobility EG
    QUEEN_MOBILITY_EG_0_VAL, QUEEN_MOBILITY_EG_1_VAL, QUEEN_MOBILITY_EG_2_VAL,
    QUEEN_MOBILITY_EG_3_VAL, QUEEN_MOBILITY_EG_4_VAL, QUEEN_MOBILITY_EG_5_VAL,
    QUEEN_MOBILITY_EG_6_VAL, QUEEN_MOBILITY_EG_7_VAL, QUEEN_MOBILITY_EG_8_VAL,
    QUEEN_MOBILITY_EG_9_VAL, QUEEN_MOBILITY_EG_10_VAL, QUEEN_MOBILITY_EG_11_VAL,
    QUEEN_MOBILITY_EG_12_VAL, QUEEN_MOBILITY_EG_13_VAL, QUEEN_MOBILITY_EG_14_VAL,
    QUEEN_MOBILITY_EG_15_VAL, QUEEN_MOBILITY_EG_16_VAL, QUEEN_MOBILITY_EG_17_VAL,
    QUEEN_MOBILITY_EG_18_VAL, QUEEN_MOBILITY_EG_19_VAL, QUEEN_MOBILITY_EG_20_VAL,
    QUEEN_MOBILITY_EG_21_VAL, QUEEN_MOBILITY_EG_22_VAL, QUEEN_MOBILITY_EG_23_VAL,
    QUEEN_MOBILITY_EG_24_VAL, QUEEN_MOBILITY_EG_25_VAL, QUEEN_MOBILITY_EG_26_VAL,
    QUEEN_MOBILITY_EG_27_VAL,

    // Pawn PST (mirrored 32 values)
    PST_PAWN_0_VAL, PST_PAWN_1_VAL, PST_PAWN_2_VAL, PST_PAWN_3_VAL,
    PST_PAWN_4_VAL, PST_PAWN_5_VAL, PST_PAWN_6_VAL, PST_PAWN_7_VAL,
    PST_PAWN_8_VAL, PST_PAWN_9_VAL, PST_PAWN_10_VAL, PST_PAWN_11_VAL,
    PST_PAWN_12_VAL, PST_PAWN_13_VAL, PST_PAWN_14_VAL, PST_PAWN_15_VAL,
    PST_PAWN_16_VAL, PST_PAWN_17_VAL, PST_PAWN_18_VAL, PST_PAWN_19_VAL,
    PST_PAWN_20_VAL, PST_PAWN_21_VAL, PST_PAWN_22_VAL, PST_PAWN_23_VAL,
    PST_PAWN_24_VAL, PST_PAWN_25_VAL, PST_PAWN_26_VAL, PST_PAWN_27_VAL,
    PST_PAWN_28_VAL, PST_PAWN_29_VAL, PST_PAWN_30_VAL, PST_PAWN_31_VAL,
    // Knight PST
    PST_KNIGHT_0_VAL, PST_KNIGHT_1_VAL, PST_KNIGHT_2_VAL, PST_KNIGHT_3_VAL,
    PST_KNIGHT_4_VAL, PST_KNIGHT_5_VAL, PST_KNIGHT_6_VAL, PST_KNIGHT_7_VAL,
    PST_KNIGHT_8_VAL, PST_KNIGHT_9_VAL, PST_KNIGHT_10_VAL, PST_KNIGHT_11_VAL,
    PST_KNIGHT_12_VAL, PST_KNIGHT_13_VAL, PST_KNIGHT_14_VAL, PST_KNIGHT_15_VAL,
    PST_KNIGHT_16_VAL, PST_KNIGHT_17_VAL, PST_KNIGHT_18_VAL, PST_KNIGHT_19_VAL,
    PST_KNIGHT_20_VAL, PST_KNIGHT_21_VAL, PST_KNIGHT_22_VAL, PST_KNIGHT_23_VAL,
    PST_KNIGHT_24_VAL, PST_KNIGHT_25_VAL, PST_KNIGHT_26_VAL, PST_KNIGHT_27_VAL,
    PST_KNIGHT_28_VAL, PST_KNIGHT_29_VAL, PST_KNIGHT_30_VAL, PST_KNIGHT_31_VAL,
    // Bishop PST
    PST_BISHOP_0_VAL, PST_BISHOP_1_VAL, PST_BISHOP_2_VAL, PST_BISHOP_3_VAL,
    PST_BISHOP_4_VAL, PST_BISHOP_5_VAL, PST_BISHOP_6_VAL, PST_BISHOP_7_VAL,
    PST_BISHOP_8_VAL, PST_BISHOP_9_VAL, PST_BISHOP_10_VAL, PST_BISHOP_11_VAL,
    PST_BISHOP_12_VAL, PST_BISHOP_13_VAL, PST_BISHOP_14_VAL, PST_BISHOP_15_VAL,
    PST_BISHOP_16_VAL, PST_BISHOP_17_VAL, PST_BISHOP_18_VAL, PST_BISHOP_19_VAL,
    PST_BISHOP_20_VAL, PST_BISHOP_21_VAL, PST_BISHOP_22_VAL, PST_BISHOP_23_VAL,
    PST_BISHOP_24_VAL, PST_BISHOP_25_VAL, PST_BISHOP_26_VAL, PST_BISHOP_27_VAL,
    PST_BISHOP_28_VAL, PST_BISHOP_29_VAL, PST_BISHOP_30_VAL, PST_BISHOP_31_VAL,
    // Rook PST
    PST_ROOK_0_VAL, PST_ROOK_1_VAL, PST_ROOK_2_VAL, PST_ROOK_3_VAL,
    PST_ROOK_4_VAL, PST_ROOK_5_VAL, PST_ROOK_6_VAL, PST_ROOK_7_VAL,
    PST_ROOK_8_VAL, PST_ROOK_9_VAL, PST_ROOK_10_VAL, PST_ROOK_11_VAL,
    PST_ROOK_12_VAL, PST_ROOK_13_VAL, PST_ROOK_14_VAL, PST_ROOK_15_VAL,
    PST_ROOK_16_VAL, PST_ROOK_17_VAL, PST_ROOK_18_VAL, PST_ROOK_19_VAL,
    PST_ROOK_20_VAL, PST_ROOK_21_VAL, PST_ROOK_22_VAL, PST_ROOK_23_VAL,
    PST_ROOK_24_VAL, PST_ROOK_25_VAL, PST_ROOK_26_VAL, PST_ROOK_27_VAL,
    PST_ROOK_28_VAL, PST_ROOK_29_VAL, PST_ROOK_30_VAL, PST_ROOK_31_VAL,
    // Queen PST
    PST_QUEEN_0_VAL, PST_QUEEN_1_VAL, PST_QUEEN_2_VAL, PST_QUEEN_3_VAL,
    PST_QUEEN_4_VAL, PST_QUEEN_5_VAL, PST_QUEEN_6_VAL, PST_QUEEN_7_VAL,
    PST_QUEEN_8_VAL, PST_QUEEN_9_VAL, PST_QUEEN_10_VAL, PST_QUEEN_11_VAL,
    PST_QUEEN_12_VAL, PST_QUEEN_13_VAL, PST_QUEEN_14_VAL, PST_QUEEN_15_VAL,
    PST_QUEEN_16_VAL, PST_QUEEN_17_VAL, PST_QUEEN_18_VAL, PST_QUEEN_19_VAL,
    PST_QUEEN_20_VAL, PST_QUEEN_21_VAL, PST_QUEEN_22_VAL, PST_QUEEN_23_VAL,
    PST_QUEEN_24_VAL, PST_QUEEN_25_VAL, PST_QUEEN_26_VAL, PST_QUEEN_27_VAL,
    PST_QUEEN_28_VAL, PST_QUEEN_29_VAL, PST_QUEEN_30_VAL, PST_QUEEN_31_VAL,
    // King MG PST
    PST_KING_MG_0_VAL, PST_KING_MG_1_VAL, PST_KING_MG_2_VAL, PST_KING_MG_3_VAL,
    PST_KING_MG_4_VAL, PST_KING_MG_5_VAL, PST_KING_MG_6_VAL, PST_KING_MG_7_VAL,
    PST_KING_MG_8_VAL, PST_KING_MG_9_VAL, PST_KING_MG_10_VAL, PST_KING_MG_11_VAL,
    PST_KING_MG_12_VAL, PST_KING_MG_13_VAL, PST_KING_MG_14_VAL, PST_KING_MG_15_VAL,
    PST_KING_MG_16_VAL, PST_KING_MG_17_VAL, PST_KING_MG_18_VAL, PST_KING_MG_19_VAL,
    PST_KING_MG_20_VAL, PST_KING_MG_21_VAL, PST_KING_MG_22_VAL, PST_KING_MG_23_VAL,
    PST_KING_MG_24_VAL, PST_KING_MG_25_VAL, PST_KING_MG_26_VAL, PST_KING_MG_27_VAL,
    PST_KING_MG_28_VAL, PST_KING_MG_29_VAL, PST_KING_MG_30_VAL, PST_KING_MG_31_VAL,
    // King EG PST
    PST_KING_EG_0_VAL, PST_KING_EG_1_VAL, PST_KING_EG_2_VAL, PST_KING_EG_3_VAL,
    PST_KING_EG_4_VAL, PST_KING_EG_5_VAL, PST_KING_EG_6_VAL, PST_KING_EG_7_VAL,
    PST_KING_EG_8_VAL, PST_KING_EG_9_VAL, PST_KING_EG_10_VAL, PST_KING_EG_11_VAL,
    PST_KING_EG_12_VAL, PST_KING_EG_13_VAL, PST_KING_EG_14_VAL, PST_KING_EG_15_VAL,
    PST_KING_EG_16_VAL, PST_KING_EG_17_VAL, PST_KING_EG_18_VAL, PST_KING_EG_19_VAL,
    PST_KING_EG_20_VAL, PST_KING_EG_21_VAL, PST_KING_EG_22_VAL, PST_KING_EG_23_VAL,
    PST_KING_EG_24_VAL, PST_KING_EG_25_VAL, PST_KING_EG_26_VAL, PST_KING_EG_27_VAL,
    PST_KING_EG_28_VAL, PST_KING_EG_29_VAL, PST_KING_EG_30_VAL, PST_KING_EG_31_VAL,

    // King Safety
    KS_PAWN_SHIELD_RANK2_VAL, KS_PAWN_SHIELD_RANK3_VAL, KS_PAWN_SHIELD_MISSING_VAL,
    KS_KNIGHT_WEIGHT_VAL, KS_BISHOP_WEIGHT_VAL, KS_ROOK_WEIGHT_VAL, KS_QUEEN_WEIGHT_VAL,
    KS_SAFETY_TABLE_0_VAL, KS_SAFETY_TABLE_1_VAL, KS_SAFETY_TABLE_2_VAL, KS_SAFETY_TABLE_3_VAL,
    KS_SAFETY_TABLE_4_VAL, KS_SAFETY_TABLE_5_VAL, KS_SAFETY_TABLE_6_VAL, KS_SAFETY_TABLE_7_VAL,
    KS_SAFETY_TABLE_8_VAL, KS_SAFETY_TABLE_9_VAL, KS_SAFETY_TABLE_10_VAL, KS_SAFETY_TABLE_11_VAL,
    KS_SAFETY_TABLE_12_VAL, KS_SAFETY_TABLE_13_VAL, KS_SAFETY_TABLE_14_VAL, KS_SAFETY_TABLE_15_VAL
};

const char* param_names[NUM_PARAMS] = {
    "piece_pawn", "piece_knight", "piece_bishop", "piece_rook", "piece_queen",
    "rook_open_file_mg", "rook_open_file_eg", "rook_semi_open_file_mg", "rook_semi_open_file_eg",
    "bishop_pair_bonus",
    "double_pawn_mg", "double_pawn_eg",
    "isolated_pawn_mg", "isolated_pawn_eg",
    "isolated_pawn_semi_open_mg", "isolated_pawn_semi_open_eg",
    "passed_pawn_mg_r1", "passed_pawn_mg_r2", "passed_pawn_mg_r3", "passed_pawn_mg_r4", "passed_pawn_mg_r5", "passed_pawn_mg_r6",
    "passed_pawn_eg_r1", "passed_pawn_eg_r2", "passed_pawn_eg_r3", "passed_pawn_eg_r4", "passed_pawn_eg_r5", "passed_pawn_eg_r6",
    "passed_pawn_defended_mg", "passed_pawn_defended_eg",
    "passed_pawn_friendly_behind_mg", "passed_pawn_friendly_behind_eg",
    "passed_pawn_enemy_behind_mg", "passed_pawn_enemy_behind_eg",
    "knight_mobility_mg_0", "knight_mobility_mg_1", "knight_mobility_mg_2", "knight_mobility_mg_3", "knight_mobility_mg_4", "knight_mobility_mg_5", "knight_mobility_mg_6", "knight_mobility_mg_7", "knight_mobility_mg_8",
    "knight_mobility_eg_0", "knight_mobility_eg_1", "knight_mobility_eg_2", "knight_mobility_eg_3", "knight_mobility_eg_4", "knight_mobility_eg_5", "knight_mobility_eg_6", "knight_mobility_eg_7", "knight_mobility_eg_8",
    "bishop_mobility_mg_0", "bishop_mobility_mg_1", "bishop_mobility_mg_2", "bishop_mobility_mg_3", "bishop_mobility_mg_4", "bishop_mobility_mg_5", "bishop_mobility_mg_6", "bishop_mobility_mg_7", "bishop_mobility_mg_8", "bishop_mobility_mg_9", "bishop_mobility_mg_10", "bishop_mobility_mg_11", "bishop_mobility_mg_12", "bishop_mobility_mg_13",
    "bishop_mobility_eg_0", "bishop_mobility_eg_1", "bishop_mobility_eg_2", "bishop_mobility_eg_3", "bishop_mobility_eg_4", "bishop_mobility_eg_5", "bishop_mobility_eg_6", "bishop_mobility_eg_7", "bishop_mobility_eg_8", "bishop_mobility_eg_9", "bishop_mobility_eg_10", "bishop_mobility_eg_11", "bishop_mobility_eg_12", "bishop_mobility_eg_13",
    "rook_mobility_mg_0", "rook_mobility_mg_1", "rook_mobility_mg_2", "rook_mobility_mg_3", "rook_mobility_mg_4", "rook_mobility_mg_5", "rook_mobility_mg_6", "rook_mobility_mg_7", "rook_mobility_mg_8", "rook_mobility_mg_9", "rook_mobility_mg_10", "rook_mobility_mg_11", "rook_mobility_mg_12", "rook_mobility_mg_13", "rook_mobility_mg_14",
    "rook_mobility_eg_0", "rook_mobility_eg_1", "rook_mobility_eg_2", "rook_mobility_eg_3", "rook_mobility_eg_4", "rook_mobility_eg_5", "rook_mobility_eg_6", "rook_mobility_eg_7", "rook_mobility_eg_8", "rook_mobility_eg_9", "rook_mobility_eg_10", "rook_mobility_eg_11", "rook_mobility_eg_12", "rook_mobility_eg_13", "rook_mobility_eg_14",
    "queen_mobility_mg_0", "queen_mobility_mg_1", "queen_mobility_mg_2", "queen_mobility_mg_3", "queen_mobility_mg_4", "queen_mobility_mg_5", "queen_mobility_mg_6", "queen_mobility_mg_7", "queen_mobility_mg_8", "queen_mobility_mg_9", "queen_mobility_mg_10", "queen_mobility_mg_11", "queen_mobility_mg_12", "queen_mobility_mg_13", "queen_mobility_mg_14", "queen_mobility_mg_15", "queen_mobility_mg_16", "queen_mobility_mg_17", "queen_mobility_mg_18", "queen_mobility_mg_19", "queen_mobility_mg_20", "queen_mobility_mg_21", "queen_mobility_mg_22", "queen_mobility_mg_23", "queen_mobility_mg_24", "queen_mobility_mg_25", "queen_mobility_mg_26", "queen_mobility_mg_27",
    "queen_mobility_eg_0", "queen_mobility_eg_1", "queen_mobility_eg_2", "queen_mobility_eg_3", "queen_mobility_eg_4", "queen_mobility_eg_5", "queen_mobility_eg_6", "queen_mobility_eg_7", "queen_mobility_eg_8", "queen_mobility_eg_9", "queen_mobility_eg_10", "queen_mobility_eg_11", "queen_mobility_eg_12", "queen_mobility_eg_13", "queen_mobility_eg_14", "queen_mobility_eg_15", "queen_mobility_eg_16", "queen_mobility_eg_17", "queen_mobility_eg_18", "queen_mobility_eg_19", "queen_mobility_eg_20", "queen_mobility_eg_21", "queen_mobility_eg_22", "queen_mobility_eg_23", "queen_mobility_eg_24", "queen_mobility_eg_25", "queen_mobility_eg_26", "queen_mobility_eg_27",
    // PST names
    "pst_pawn_0", "pst_pawn_1", "pst_pawn_2", "pst_pawn_3", "pst_pawn_4", "pst_pawn_5", "pst_pawn_6", "pst_pawn_7",
    "pst_pawn_8", "pst_pawn_9", "pst_pawn_10", "pst_pawn_11", "pst_pawn_12", "pst_pawn_13", "pst_pawn_14", "pst_pawn_15",
    "pst_pawn_16", "pst_pawn_17", "pst_pawn_18", "pst_pawn_19", "pst_pawn_20", "pst_pawn_21", "pst_pawn_22", "pst_pawn_23",
    "pst_pawn_24", "pst_pawn_25", "pst_pawn_26", "pst_pawn_27", "pst_pawn_28", "pst_pawn_29", "pst_pawn_30", "pst_pawn_31",
    "pst_knight_0", "pst_knight_1", "pst_knight_2", "pst_knight_3", "pst_knight_4", "pst_knight_5", "pst_knight_6", "pst_knight_7",
    "pst_knight_8", "pst_knight_9", "pst_knight_10", "pst_knight_11", "pst_knight_12", "pst_knight_13", "pst_knight_14", "pst_knight_15",
    "pst_knight_16", "pst_knight_17", "pst_knight_18", "pst_knight_19", "pst_knight_20", "pst_knight_21", "pst_knight_22", "pst_knight_23",
    "pst_knight_24", "pst_knight_25", "pst_knight_26", "pst_knight_27", "pst_knight_28", "pst_knight_29", "pst_knight_30", "pst_knight_31",
    "pst_bishop_0", "pst_bishop_1", "pst_bishop_2", "pst_bishop_3", "pst_bishop_4", "pst_bishop_5", "pst_bishop_6", "pst_bishop_7",
    "pst_bishop_8", "pst_bishop_9", "pst_bishop_10", "pst_bishop_11", "pst_bishop_12", "pst_bishop_13", "pst_bishop_14", "pst_bishop_15",
    "pst_bishop_16", "pst_bishop_17", "pst_bishop_18", "pst_bishop_19", "pst_bishop_20", "pst_bishop_21", "pst_bishop_22", "pst_bishop_23",
    "pst_bishop_24", "pst_bishop_25", "pst_bishop_26", "pst_bishop_27", "pst_bishop_28", "pst_bishop_29", "pst_bishop_30", "pst_bishop_31",
    "pst_rook_0", "pst_rook_1", "pst_rook_2", "pst_rook_3", "pst_rook_4", "pst_rook_5", "pst_rook_6", "pst_rook_7",
    "pst_rook_8", "pst_rook_9", "pst_rook_10", "pst_rook_11", "pst_rook_12", "pst_rook_13", "pst_rook_14", "pst_rook_15",
    "pst_rook_16", "pst_rook_17", "pst_rook_18", "pst_rook_19", "pst_rook_20", "pst_rook_21", "pst_rook_22", "pst_rook_23",
    "pst_rook_24", "pst_rook_25", "pst_rook_26", "pst_rook_27", "pst_rook_28", "pst_rook_29", "pst_rook_30", "pst_rook_31",
    "pst_queen_0", "pst_queen_1", "pst_queen_2", "pst_queen_3", "pst_queen_4", "pst_queen_5", "pst_queen_6", "pst_queen_7",
    "pst_queen_8", "pst_queen_9", "pst_queen_10", "pst_queen_11", "pst_queen_12", "pst_queen_13", "pst_queen_14", "pst_queen_15",
    "pst_queen_16", "pst_queen_17", "pst_queen_18", "pst_queen_19", "pst_queen_20", "pst_queen_21", "pst_queen_22", "pst_queen_23",
    "pst_queen_24", "pst_queen_25", "pst_queen_26", "pst_queen_27", "pst_queen_28", "pst_queen_29", "pst_queen_30", "pst_queen_31",
    "pst_king_mg_0", "pst_king_mg_1", "pst_king_mg_2", "pst_king_mg_3", "pst_king_mg_4", "pst_king_mg_5", "pst_king_mg_6", "pst_king_mg_7",
    "pst_king_mg_8", "pst_king_mg_9", "pst_king_mg_10", "pst_king_mg_11", "pst_king_mg_12", "pst_king_mg_13", "pst_king_mg_14", "pst_king_mg_15",
    "pst_king_mg_16", "pst_king_mg_17", "pst_king_mg_18", "pst_king_mg_19", "pst_king_mg_20", "pst_king_mg_21", "pst_king_mg_22", "pst_king_mg_23",
    "pst_king_mg_24", "pst_king_mg_25", "pst_king_mg_26", "pst_king_mg_27", "pst_king_mg_28", "pst_king_mg_29", "pst_king_mg_30", "pst_king_mg_31",
    "pst_king_eg_0", "pst_king_eg_1", "pst_king_eg_2", "pst_king_eg_3", "pst_king_eg_4", "pst_king_eg_5", "pst_king_eg_6", "pst_king_eg_7",
    "pst_king_eg_8", "pst_king_eg_9", "pst_king_eg_10", "pst_king_eg_11", "pst_king_eg_12", "pst_king_eg_13", "pst_king_eg_14", "pst_king_eg_15",
    "pst_king_eg_16", "pst_king_eg_17", "pst_king_eg_18", "pst_king_eg_19", "pst_king_eg_20", "pst_king_eg_21", "pst_king_eg_22", "pst_king_eg_23",
    "pst_king_eg_24", "pst_king_eg_25", "pst_king_eg_26", "pst_king_eg_27", "pst_king_eg_28", "pst_king_eg_29", "pst_king_eg_30", "pst_king_eg_31",
    // King Safety names
    "ks_pawn_shield_rank2", "ks_pawn_shield_rank3", "ks_pawn_shield_missing",
    "ks_knight_weight", "ks_bishop_weight", "ks_rook_weight", "ks_queen_weight",
    "ks_safety_table_0", "ks_safety_table_1", "ks_safety_table_2", "ks_safety_table_3",
    "ks_safety_table_4", "ks_safety_table_5", "ks_safety_table_6", "ks_safety_table_7",
    "ks_safety_table_8", "ks_safety_table_9", "ks_safety_table_10", "ks_safety_table_11",
    "ks_safety_table_12", "ks_safety_table_13", "ks_safety_table_14", "ks_safety_table_15"
};

// Cached file masks for open file detection
static const uint64_t file_masks[8] = {
    0x0101010101010101ULL << 0, 0x0101010101010101ULL << 1,
    0x0101010101010101ULL << 2, 0x0101010101010101ULL << 3,
    0x0101010101010101ULL << 4, 0x0101010101010101ULL << 5,
    0x0101010101010101ULL << 6, 0x0101010101010101ULL << 7
};

/* ================================================================
 * King safety feature extraction helper.
 * Computes pawn shield features and linearized safety table features.
 * sign: +1 for white defender, -1 for black defender.
 * ================================================================ */
static void extract_king_safety_features(const Position *pos, Color defender,
                                          double *features, double mg_scale, double sign) {
    int side = COLOR_IDX(defender);
    Color them = OPPOSITE(defender);
    int enemy_side = COLOR_IDX(them);
    int king_sq = pos->kingSq[side];
    int king_file = FILE_OF(king_sq);
    int king_rank = RANK_OF(king_sq);

    // 1. Pawn Shield
    int relative_king_rank = (defender == WHITE) ? king_rank : (7 - king_rank);
    if (relative_king_rank <= 2) {
        int start_file, end_file;
        if (king_file >= FILE_F) { start_file = FILE_F; end_file = FILE_H; }
        else if (king_file <= FILE_C) { start_file = FILE_A; end_file = FILE_C; }
        else { start_file = -1; end_file = -1; }

        if (start_file != -1) {
            uint64_t pawns = pos->pieces[side][PAWN];
            int r2 = (defender == WHITE) ? RANK_2 : RANK_7;
            int r3 = (defender == WHITE) ? RANK_3 : RANK_6;
            for (int f = start_file; f <= end_file; ++f) {
                uint64_t sq_r2 = SQUARE(f, r2);
                uint64_t sq_r3 = SQUARE(f, r3);
                if (pawns & (1ULL << sq_r2)) {
                    features[PARAM_KS_PAWN_SHIELD_RANK2] += sign * mg_scale;
                } else if (pawns & (1ULL << sq_r3)) {
                    features[PARAM_KS_PAWN_SHIELD_RANK3] += sign * mg_scale;
                } else {
                    features[PARAM_KS_PAWN_SHIELD_MISSING] += sign * mg_scale;
                }
            }
        }
    }

    // 2. King Attacks — linearize safety_table entries
    // Use initial/fixed attack weights for computing attacker_count and attack_weight
    int attacker_count = 0;
    int attack_weight = 0;
    uint64_t occ = pos->occAll;
    uint64_t king_zone = kingAttacks[king_sq] | (1ULL << king_sq);

    int knight_attackers = bit_count(pos->pieces[enemy_side][KNIGHT] & knightAttacks[king_sq]);
    /* Approximate: check if any knight attacks the king zone.
       For exact matching with king_safety.c we'd need the precomputed tables,
       but those are static in king_safety.c. Use direct attack check instead. */
    {
        uint64_t knights = pos->pieces[enemy_side][KNIGHT];
        int na = 0;
        while (knights) {
            int sq = pop_lsb(&knights);
            if (knightAttacks[sq] & king_zone) na++;
        }
        knight_attackers = na;
    }
    attacker_count += knight_attackers;
    attack_weight += knight_attackers * KS_KNIGHT_WEIGHT_VAL;

    int bishop_attackers = 0;
    {
        uint64_t bishops = pos->pieces[enemy_side][BISHOP];
        while (bishops) {
            int sq = pop_lsb(&bishops);
            if (bishopAttacks(sq, occ) & king_zone) {
                bishop_attackers++;
            }
        }
    }
    attacker_count += bishop_attackers;
    attack_weight += bishop_attackers * KS_BISHOP_WEIGHT_VAL;

    int rook_attackers = 0;
    {
        uint64_t rooks = pos->pieces[enemy_side][ROOK];
        while (rooks) {
            int sq = pop_lsb(&rooks);
            if (rookAttacks(sq, occ) & king_zone) {
                rook_attackers++;
            }
        }
    }
    attacker_count += rook_attackers;
    attack_weight += rook_attackers * KS_ROOK_WEIGHT_VAL;

    int queen_attackers = 0;
    {
        uint64_t queens = pos->pieces[enemy_side][QUEEN];
        while (queens) {
            int sq = pop_lsb(&queens);
            if (queenAttacks(sq, occ) & king_zone) {
                queen_attackers++;
            }
        }
    }
    attacker_count += queen_attackers;
    attack_weight += queen_attackers * KS_QUEEN_WEIGHT_VAL;

    if (attacker_count >= 2) {
        if (attacker_count > 15) attacker_count = 15;
        int has_queen = (pos->pieces[enemy_side][QUEEN] != 0);
        int divisor = has_queen ? 8 : 16;
        /* Feature for safety_table[attacker_count]:
         * penalty = safety_table[attacker_count] * attack_weight / divisor
         * This is linear in safety_table[attacker_count], so the coefficient is:
         * attack_weight / divisor
         * The penalty is SUBTRACTED from the defender's score. */
        double coeff = (double)attack_weight / (double)divisor;
        features[PARAM_KS_SAFETY_TABLE_0 + attacker_count] -= sign * mg_scale * coeff;
    }
}


/* ================================================================
 * Feature extraction: decomposes the evaluation into linear features.
 * PST values are now features instead of const_score.
 * King safety is linearized.
 * ================================================================ */
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
    uint64_t w_pawns = w_pawns_copy;
    while (w_pawns) {
        int sq = pop_lsb(&w_pawns);
        features[PARAM_PST_PAWN_0 + pst_mirror_idx(sq)] += 1.0;
        features[PARAM_PIECE_PAWN] += 1.0;

        if (!(adjacentFilesMask[sq] & w_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & b_pawns_copy);
            features[is_semi_open ? PARAM_ISOLATED_PAWN_SEMI_OPEN_MG : PARAM_ISOLATED_PAWN_MG] -= mg_scale;
            features[is_semi_open ? PARAM_ISOLATED_PAWN_SEMI_OPEN_EG : PARAM_ISOLATED_PAWN_EG] -= eg_scale;
        }
        if (fileBehindMasks[0][sq] & w_pawns_copy) {
            features[PARAM_DOUBLE_PAWN_MG] -= mg_scale;
            features[PARAM_DOUBLE_PAWN_EG] -= eg_scale;
        }
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

    uint64_t b_pawns = b_pawns_copy;
    while (b_pawns) {
        int sq = pop_lsb(&b_pawns);
        features[PARAM_PST_PAWN_0 + pst_mirror_idx(sq ^ 56)] -= 1.0;
        features[PARAM_PIECE_PAWN] -= 1.0;

        if (!(adjacentFilesMask[sq] & b_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & w_pawns_copy);
            features[is_semi_open ? PARAM_ISOLATED_PAWN_SEMI_OPEN_MG : PARAM_ISOLATED_PAWN_MG] += mg_scale;
            features[is_semi_open ? PARAM_ISOLATED_PAWN_SEMI_OPEN_EG : PARAM_ISOLATED_PAWN_EG] += eg_scale;
        }
        if (fileBehindMasks[1][sq] & b_pawns_copy) {
            features[PARAM_DOUBLE_PAWN_MG] += mg_scale;
            features[PARAM_DOUBLE_PAWN_EG] += eg_scale;
        }
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
        features[PARAM_PST_KNIGHT_0 + pst_mirror_idx(sq)] += 1.0;
        features[PARAM_PIECE_KNIGHT] += 1.0;
    }
    uint64_t b_knights = pos->pieces[COLOR_IDX(BLACK)][KNIGHT];
    while (b_knights) {
        int sq = pop_lsb(&b_knights);
        features[PARAM_PST_KNIGHT_0 + pst_mirror_idx(sq ^ 56)] -= 1.0;
        features[PARAM_PIECE_KNIGHT] -= 1.0;
    }

    // --- Bishops ---
    uint64_t w_bishops = pos->pieces[COLOR_IDX(WHITE)][BISHOP];
    while (w_bishops) {
        int sq = pop_lsb(&w_bishops);
        features[PARAM_PST_BISHOP_0 + pst_mirror_idx(sq)] += 1.0;
        features[PARAM_PIECE_BISHOP] += 1.0;
    }
    uint64_t b_bishops = pos->pieces[COLOR_IDX(BLACK)][BISHOP];
    while (b_bishops) {
        int sq = pop_lsb(&b_bishops);
        features[PARAM_PST_BISHOP_0 + pst_mirror_idx(sq ^ 56)] -= 1.0;
        features[PARAM_PIECE_BISHOP] -= 1.0;
    }

    // --- Rooks ---
    uint64_t w_rooks = pos->pieces[COLOR_IDX(WHITE)][ROOK];
    while (w_rooks) {
        int sq = pop_lsb(&w_rooks);
        features[PARAM_PST_ROOK_0 + pst_mirror_idx(sq)] += 1.0;
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
        features[PARAM_PST_ROOK_0 + pst_mirror_idx(sq ^ 56)] -= 1.0;
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
        features[PARAM_PST_QUEEN_0 + pst_mirror_idx(sq)] += 1.0;
        features[PARAM_PIECE_QUEEN] += 1.0;
    }
    uint64_t b_queens = pos->pieces[COLOR_IDX(BLACK)][QUEEN];
    while (b_queens) {
        int sq = pop_lsb(&b_queens);
        features[PARAM_PST_QUEEN_0 + pst_mirror_idx(sq ^ 56)] -= 1.0;
        features[PARAM_PIECE_QUEEN] -= 1.0;
    }

    // --- Kings (PST as features, material value as const_score) ---
    {
        int sq = pos->kingSq[COLOR_IDX(WHITE)];
        *const_score += 20000;
        features[PARAM_PST_KING_MG_0 + pst_mirror_idx(sq)] += mg_scale;
        features[PARAM_PST_KING_EG_0 + pst_mirror_idx(sq)] += eg_scale;
    }
    {
        int sq = pos->kingSq[COLOR_IDX(BLACK)];
        *const_score -= 20000;
        features[PARAM_PST_KING_MG_0 + pst_mirror_idx(sq ^ 56)] -= mg_scale;
        features[PARAM_PST_KING_EG_0 + pst_mirror_idx(sq ^ 56)] -= eg_scale;
    }

    // --- Bishop Pair ---
    if (bit_count(pos->pieces[COLOR_IDX(WHITE)][BISHOP]) >= 2) {
        features[PARAM_BISHOP_PAIR_BONUS] += 1.0;
    }
    if (bit_count(pos->pieces[COLOR_IDX(BLACK)][BISHOP]) >= 2) {
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

        uint64_t knights = pos->pieces[COLOR_IDX(us)][KNIGHT];
        while (knights) {
            int sq = pop_lsb(&knights);
            uint64_t attacks = knightAttacks[sq] & mobility_area;
            int count = bit_count(attacks);
            if (count > 8) count = 8;
            features[PARAM_KNIGHT_MOBILITY_MG_0 + count] += mg_scale;
            features[PARAM_KNIGHT_MOBILITY_EG_0 + count] += eg_scale;
        }
        uint64_t bishops = pos->pieces[COLOR_IDX(us)][BISHOP];
        while (bishops) {
            int sq = pop_lsb(&bishops);
            uint64_t attacks = bishopAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 13) count = 13;
            features[PARAM_BISHOP_MOBILITY_MG_0 + count] += mg_scale;
            features[PARAM_BISHOP_MOBILITY_EG_0 + count] += eg_scale;
        }
        uint64_t rooks = pos->pieces[COLOR_IDX(us)][ROOK];
        while (rooks) {
            int sq = pop_lsb(&rooks);
            uint64_t attacks = rookAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 14) count = 14;
            features[PARAM_ROOK_MOBILITY_MG_0 + count] += mg_scale;
            features[PARAM_ROOK_MOBILITY_EG_0 + count] += eg_scale;
        }
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

        uint64_t knights = pos->pieces[COLOR_IDX(us)][KNIGHT];
        while (knights) {
            int sq = pop_lsb(&knights);
            uint64_t attacks = knightAttacks[sq] & mobility_area;
            int count = bit_count(attacks);
            if (count > 8) count = 8;
            features[PARAM_KNIGHT_MOBILITY_MG_0 + count] -= mg_scale;
            features[PARAM_KNIGHT_MOBILITY_EG_0 + count] -= eg_scale;
        }
        uint64_t bishops = pos->pieces[COLOR_IDX(us)][BISHOP];
        while (bishops) {
            int sq = pop_lsb(&bishops);
            uint64_t attacks = bishopAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 13) count = 13;
            features[PARAM_BISHOP_MOBILITY_MG_0 + count] -= mg_scale;
            features[PARAM_BISHOP_MOBILITY_EG_0 + count] -= eg_scale;
        }
        uint64_t rooks = pos->pieces[COLOR_IDX(us)][ROOK];
        while (rooks) {
            int sq = pop_lsb(&rooks);
            uint64_t attacks = rookAttacks(sq, occ) & mobility_area;
            int count = bit_count(attacks);
            if (count > 14) count = 14;
            features[PARAM_ROOK_MOBILITY_MG_0 + count] -= mg_scale;
            features[PARAM_ROOK_MOBILITY_EG_0 + count] -= eg_scale;
        }
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

    // --- King Safety ---
    extract_king_safety_features(pos, WHITE, features, mg_scale, +1.0);
    extract_king_safety_features(pos, BLACK, features, mg_scale, -1.0);
}


/* ================================================================
 * Replicated evaluation using integer parameters.
 * Must match evaluate() exactly for verification.
 * ================================================================ */
int evaluate_replicated(const Position *pos, const int *params) {
    int mg_score = 0;
    int eg_score = 0;
    int phase = get_game_phase(pos);
    if (phase > 24) phase = 24;

    /* Build PST lookup tables from params */
    int pst_pawn[64], pst_knight[64], pst_bishop[64], pst_rook[64], pst_queen[64];
    int pst_king_mg[64], pst_king_eg[64];
    for (int sq = 0; sq < 64; sq++) {
        int midx = pst_mirror_idx(sq);
        pst_pawn[sq]    = params[PARAM_PST_PAWN_0 + midx];
        pst_knight[sq]  = params[PARAM_PST_KNIGHT_0 + midx];
        pst_bishop[sq]  = params[PARAM_PST_BISHOP_0 + midx];
        pst_rook[sq]    = params[PARAM_PST_ROOK_0 + midx];
        pst_queen[sq]   = params[PARAM_PST_QUEEN_0 + midx];
        pst_king_mg[sq] = params[PARAM_PST_KING_MG_0 + midx];
        pst_king_eg[sq] = params[PARAM_PST_KING_EG_0 + midx];
    }

    uint64_t w_pawns_copy = pos->pieces[COLOR_IDX(WHITE)][PAWN];
    uint64_t b_pawns_copy = pos->pieces[COLOR_IDX(BLACK)][PAWN];
    uint64_t w_rooks_queens = pos->pieces[COLOR_IDX(WHITE)][ROOK] | pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    uint64_t b_rooks_queens = pos->pieces[COLOR_IDX(BLACK)][ROOK] | pos->pieces[COLOR_IDX(BLACK)][QUEEN];

    // --- Pawns ---
    uint64_t w_pawns = w_pawns_copy;
    while (w_pawns) {
        int sq = pop_lsb(&w_pawns);
        int item_mg = params[PARAM_PIECE_PAWN] + pst_pawn[sq];
        int item_eg = params[PARAM_PIECE_PAWN] + pst_pawn[sq];
        if (!(adjacentFilesMask[sq] & w_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & b_pawns_copy);
            item_mg -= is_semi_open ? params[PARAM_ISOLATED_PAWN_SEMI_OPEN_MG] : params[PARAM_ISOLATED_PAWN_MG];
            item_eg -= is_semi_open ? params[PARAM_ISOLATED_PAWN_SEMI_OPEN_EG] : params[PARAM_ISOLATED_PAWN_EG];
        }
        if (fileBehindMasks[0][sq] & w_pawns_copy) {
            item_mg -= params[PARAM_DOUBLE_PAWN_MG];
            item_eg -= params[PARAM_DOUBLE_PAWN_EG];
        }
        if (!(passedPawnMasks[COLOR_IDX(WHITE)][sq] & b_pawns_copy)) {
            int rank = RANK_OF(sq);
            int bp_mg = params[PARAM_PASSED_PAWN_MG_R1 + rank - 1];
            int bp_eg = params[PARAM_PASSED_PAWN_EG_R1 + rank - 1];
            if (pawnAttacks[COLOR_IDX(BLACK)][sq] & w_pawns_copy) {
                bp_mg += params[PARAM_PASSED_PAWN_DEFENDED_MG];
                bp_eg += params[PARAM_PASSED_PAWN_DEFENDED_EG];
            }
            if (pos->occAll & (1ULL << (sq + 8))) {
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
            item_mg += bp_mg;
            item_eg += bp_eg;
        }
        mg_score += item_mg;
        eg_score += item_eg;
    }

    uint64_t b_pawns = b_pawns_copy;
    while (b_pawns) {
        int sq = pop_lsb(&b_pawns);
        int item_mg = params[PARAM_PIECE_PAWN] + pst_pawn[sq ^ 56];
        int item_eg = params[PARAM_PIECE_PAWN] + pst_pawn[sq ^ 56];
        if (!(adjacentFilesMask[sq] & b_pawns_copy)) {
            int is_semi_open = !(file_masks[sq & 7] & w_pawns_copy);
            item_mg -= is_semi_open ? params[PARAM_ISOLATED_PAWN_SEMI_OPEN_MG] : params[PARAM_ISOLATED_PAWN_MG];
            item_eg -= is_semi_open ? params[PARAM_ISOLATED_PAWN_SEMI_OPEN_EG] : params[PARAM_ISOLATED_PAWN_EG];
        }
        if (fileBehindMasks[1][sq] & b_pawns_copy) {
            item_mg -= params[PARAM_DOUBLE_PAWN_MG];
            item_eg -= params[PARAM_DOUBLE_PAWN_EG];
        }
        if (!(passedPawnMasks[COLOR_IDX(BLACK)][sq] & w_pawns_copy)) {
            int rank = 7 - RANK_OF(sq);
            int bp_mg = params[PARAM_PASSED_PAWN_MG_R1 + rank - 1];
            int bp_eg = params[PARAM_PASSED_PAWN_EG_R1 + rank - 1];
            if (pawnAttacks[COLOR_IDX(WHITE)][sq] & b_pawns_copy) {
                bp_mg += params[PARAM_PASSED_PAWN_DEFENDED_MG];
                bp_eg += params[PARAM_PASSED_PAWN_DEFENDED_EG];
            }
            if (pos->occAll & (1ULL << (sq - 8))) {
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
            item_mg += bp_mg;
            item_eg += bp_eg;
        }
        mg_score -= item_mg;
        eg_score -= item_eg;
    }

    // --- Knights ---
    uint64_t w_knights = pos->pieces[COLOR_IDX(WHITE)][KNIGHT];
    while (w_knights) {
        int sq = pop_lsb(&w_knights);
        int val = params[PARAM_PIECE_KNIGHT] + pst_knight[sq];
        mg_score += val; eg_score += val;
    }
    uint64_t b_knights = pos->pieces[COLOR_IDX(BLACK)][KNIGHT];
    while (b_knights) {
        int sq = pop_lsb(&b_knights);
        int val = params[PARAM_PIECE_KNIGHT] + pst_knight[sq ^ 56];
        mg_score -= val; eg_score -= val;
    }

    // --- Bishops ---
    uint64_t w_bishops = pos->pieces[COLOR_IDX(WHITE)][BISHOP];
    while (w_bishops) {
        int sq = pop_lsb(&w_bishops);
        int val = params[PARAM_PIECE_BISHOP] + pst_bishop[sq];
        mg_score += val; eg_score += val;
    }
    uint64_t b_bishops = pos->pieces[COLOR_IDX(BLACK)][BISHOP];
    while (b_bishops) {
        int sq = pop_lsb(&b_bishops);
        int val = params[PARAM_PIECE_BISHOP] + pst_bishop[sq ^ 56];
        mg_score -= val; eg_score -= val;
    }

    // --- Rooks ---
    uint64_t w_rooks = pos->pieces[COLOR_IDX(WHITE)][ROOK];
    while (w_rooks) {
        int sq = pop_lsb(&w_rooks);
        int item_mg = params[PARAM_PIECE_ROOK] + pst_rook[sq];
        int item_eg = params[PARAM_PIECE_ROOK] + pst_rook[sq];
        int file = sq & 7;
        if (!(file_masks[file] & w_pawns_copy)) {
            if (!(file_masks[file] & b_pawns_copy)) {
                item_mg += params[PARAM_ROOK_OPEN_FILE_MG]; item_eg += params[PARAM_ROOK_OPEN_FILE_EG];
            } else {
                item_mg += params[PARAM_ROOK_SEMI_OPEN_FILE_MG]; item_eg += params[PARAM_ROOK_SEMI_OPEN_FILE_EG];
            }
        }
        mg_score += item_mg; eg_score += item_eg;
    }
    uint64_t b_rooks = pos->pieces[COLOR_IDX(BLACK)][ROOK];
    while (b_rooks) {
        int sq = pop_lsb(&b_rooks);
        int item_mg = params[PARAM_PIECE_ROOK] + pst_rook[sq ^ 56];
        int item_eg = params[PARAM_PIECE_ROOK] + pst_rook[sq ^ 56];
        int file = sq & 7;
        if (!(file_masks[file] & b_pawns_copy)) {
            if (!(file_masks[file] & w_pawns_copy)) {
                item_mg += params[PARAM_ROOK_OPEN_FILE_MG]; item_eg += params[PARAM_ROOK_OPEN_FILE_EG];
            } else {
                item_mg += params[PARAM_ROOK_SEMI_OPEN_FILE_MG]; item_eg += params[PARAM_ROOK_SEMI_OPEN_FILE_EG];
            }
        }
        mg_score -= item_mg; eg_score -= item_eg;
    }

    // --- Queens ---
    uint64_t w_queens = pos->pieces[COLOR_IDX(WHITE)][QUEEN];
    while (w_queens) {
        int sq = pop_lsb(&w_queens);
        int val = params[PARAM_PIECE_QUEEN] + pst_queen[sq];
        mg_score += val; eg_score += val;
    }
    uint64_t b_queens = pos->pieces[COLOR_IDX(BLACK)][QUEEN];
    while (b_queens) {
        int sq = pop_lsb(&b_queens);
        int val = params[PARAM_PIECE_QUEEN] + pst_queen[sq ^ 56];
        mg_score -= val; eg_score -= val;
    }

    // --- Kings ---
    {
        int sq = pos->kingSq[COLOR_IDX(WHITE)];
        mg_score += 20000 + pst_king_mg[sq];
        eg_score += 20000 + pst_king_eg[sq];
    }
    {
        int sq = pos->kingSq[COLOR_IDX(BLACK)];
        mg_score -= 20000 + pst_king_mg[sq ^ 56];
        eg_score -= 20000 + pst_king_eg[sq ^ 56];
    }

    // --- Bishop Pair ---
    if (bit_count(pos->pieces[COLOR_IDX(WHITE)][BISHOP]) >= 2) {
        mg_score += params[PARAM_BISHOP_PAIR_BONUS];
        eg_score += params[PARAM_BISHOP_PAIR_BONUS];
    }
    if (bit_count(pos->pieces[COLOR_IDX(BLACK)][BISHOP]) >= 2) {
        mg_score -= params[PARAM_BISHOP_PAIR_BONUS];
        eg_score -= params[PARAM_BISHOP_PAIR_BONUS];
    }

    // --- Mobility ---
    uint64_t occ = pos->occAll;
    int white_mob_mg = 0, white_mob_eg = 0;
    {
        uint64_t own_pieces = pos->occ[COLOR_IDX(WHITE)];
        uint64_t enemy_pawns = pos->pieces[COLOR_IDX(BLACK)][PAWN];
        uint64_t enemy_pawn_attacks = ((enemy_pawns & ~0x8080808080808080ULL) >> 7) |
                                     ((enemy_pawns & ~0x0101010101010101ULL) >> 9);
        uint64_t mobility_area = ~own_pieces & ~enemy_pawn_attacks;

        uint64_t knights = pos->pieces[COLOR_IDX(WHITE)][KNIGHT];
        while (knights) { int sq = pop_lsb(&knights); uint64_t a = knightAttacks[sq] & mobility_area; int c = bit_count(a); if (c > 8) c = 8; white_mob_mg += params[PARAM_KNIGHT_MOBILITY_MG_0 + c]; white_mob_eg += params[PARAM_KNIGHT_MOBILITY_EG_0 + c]; }
        uint64_t bishops = pos->pieces[COLOR_IDX(WHITE)][BISHOP];
        while (bishops) { int sq = pop_lsb(&bishops); uint64_t a = bishopAttacks(sq, occ) & mobility_area; int c = bit_count(a); if (c > 13) c = 13; white_mob_mg += params[PARAM_BISHOP_MOBILITY_MG_0 + c]; white_mob_eg += params[PARAM_BISHOP_MOBILITY_EG_0 + c]; }
        uint64_t rooks = pos->pieces[COLOR_IDX(WHITE)][ROOK];
        while (rooks) { int sq = pop_lsb(&rooks); uint64_t a = rookAttacks(sq, occ) & mobility_area; int c = bit_count(a); if (c > 14) c = 14; white_mob_mg += params[PARAM_ROOK_MOBILITY_MG_0 + c]; white_mob_eg += params[PARAM_ROOK_MOBILITY_EG_0 + c]; }
        uint64_t queens = pos->pieces[COLOR_IDX(WHITE)][QUEEN];
        while (queens) { int sq = pop_lsb(&queens); uint64_t a = queenAttacks(sq, occ) & mobility_area; int c = bit_count(a); if (c > 27) c = 27; white_mob_mg += params[PARAM_QUEEN_MOBILITY_MG_0 + c]; white_mob_eg += params[PARAM_QUEEN_MOBILITY_EG_0 + c]; }
    }
    int black_mob_mg = 0, black_mob_eg = 0;
    {
        uint64_t own_pieces = pos->occ[COLOR_IDX(BLACK)];
        uint64_t enemy_pawns = pos->pieces[COLOR_IDX(WHITE)][PAWN];
        uint64_t enemy_pawn_attacks = ((enemy_pawns & ~0x8080808080808080ULL) << 9) |
                                     ((enemy_pawns & ~0x0101010101010101ULL) << 7);
        uint64_t mobility_area = ~own_pieces & ~enemy_pawn_attacks;

        uint64_t knights = pos->pieces[COLOR_IDX(BLACK)][KNIGHT];
        while (knights) { int sq = pop_lsb(&knights); uint64_t a = knightAttacks[sq] & mobility_area; int c = bit_count(a); if (c > 8) c = 8; black_mob_mg += params[PARAM_KNIGHT_MOBILITY_MG_0 + c]; black_mob_eg += params[PARAM_KNIGHT_MOBILITY_EG_0 + c]; }
        uint64_t bishops = pos->pieces[COLOR_IDX(BLACK)][BISHOP];
        while (bishops) { int sq = pop_lsb(&bishops); uint64_t a = bishopAttacks(sq, occ) & mobility_area; int c = bit_count(a); if (c > 13) c = 13; black_mob_mg += params[PARAM_BISHOP_MOBILITY_MG_0 + c]; black_mob_eg += params[PARAM_BISHOP_MOBILITY_EG_0 + c]; }
        uint64_t rooks = pos->pieces[COLOR_IDX(BLACK)][ROOK];
        while (rooks) { int sq = pop_lsb(&rooks); uint64_t a = rookAttacks(sq, occ) & mobility_area; int c = bit_count(a); if (c > 14) c = 14; black_mob_mg += params[PARAM_ROOK_MOBILITY_MG_0 + c]; black_mob_eg += params[PARAM_ROOK_MOBILITY_EG_0 + c]; }
        uint64_t queens = pos->pieces[COLOR_IDX(BLACK)][QUEEN];
        while (queens) { int sq = pop_lsb(&queens); uint64_t a = queenAttacks(sq, occ) & mobility_area; int c = bit_count(a); if (c > 27) c = 27; black_mob_mg += params[PARAM_QUEEN_MOBILITY_MG_0 + c]; black_mob_eg += params[PARAM_QUEEN_MOBILITY_EG_0 + c]; }
    }
    mg_score += white_mob_mg - black_mob_mg;
    eg_score += white_mob_eg - black_mob_eg;

    // --- King Safety (replicated using params) ---
    int ks_score = 0;
    for (int color_iter = 0; color_iter < 2; color_iter++) {
        Color defender = (color_iter == 0) ? WHITE : BLACK;
        int side_idx = COLOR_IDX(defender);
        Color them = OPPOSITE(defender);
        int enemy_idx = COLOR_IDX(them);
        int king_sq = pos->kingSq[side_idx];
        int king_file = FILE_OF(king_sq);
        int king_rank = RANK_OF(king_sq);

        int shield_score = 0;
        int relative_king_rank = (defender == WHITE) ? king_rank : (7 - king_rank);
        if (relative_king_rank <= 2) {
            int start_file, end_file;
            if (king_file >= FILE_F) { start_file = FILE_F; end_file = FILE_H; }
            else if (king_file <= FILE_C) { start_file = FILE_A; end_file = FILE_C; }
            else { start_file = -1; end_file = -1; }
            if (start_file != -1) {
                uint64_t pawns = pos->pieces[side_idx][PAWN];
                int r2 = (defender == WHITE) ? RANK_2 : RANK_7;
                int r3 = (defender == WHITE) ? RANK_3 : RANK_6;
                for (int f = start_file; f <= end_file; ++f) {
                    uint64_t sq_r2 = SQUARE(f, r2);
                    uint64_t sq_r3 = SQUARE(f, r3);
                    if (pawns & (1ULL << sq_r2)) shield_score += params[PARAM_KS_PAWN_SHIELD_RANK2];
                    else if (pawns & (1ULL << sq_r3)) shield_score += params[PARAM_KS_PAWN_SHIELD_RANK3];
                    else shield_score += params[PARAM_KS_PAWN_SHIELD_MISSING];
                }
            }
        }

        int attacker_count = 0;
        int attack_weight = 0;
        uint64_t king_zone = kingAttacks[king_sq] | (1ULL << king_sq);

        uint64_t kn = pos->pieces[enemy_idx][KNIGHT];
        while (kn) { int sq = pop_lsb(&kn); if (knightAttacks[sq] & king_zone) { attacker_count++; attack_weight += params[PARAM_KS_KNIGHT_WEIGHT]; } }
        uint64_t bi = pos->pieces[enemy_idx][BISHOP];
        while (bi) { int sq = pop_lsb(&bi); if (bishopAttacks(sq, pos->occAll) & king_zone) { attacker_count++; attack_weight += params[PARAM_KS_BISHOP_WEIGHT]; } }
        uint64_t ro = pos->pieces[enemy_idx][ROOK];
        while (ro) { int sq = pop_lsb(&ro); if (rookAttacks(sq, pos->occAll) & king_zone) { attacker_count++; attack_weight += params[PARAM_KS_ROOK_WEIGHT]; } }
        uint64_t qu = pos->pieces[enemy_idx][QUEEN];
        while (qu) { int sq = pop_lsb(&qu); if (queenAttacks(sq, pos->occAll) & king_zone) { attacker_count++; attack_weight += params[PARAM_KS_QUEEN_WEIGHT]; } }

        int penalty = 0;
        if (attacker_count >= 2) {
            if (attacker_count > 15) attacker_count = 15;
            penalty = (params[PARAM_KS_SAFETY_TABLE_0 + attacker_count] * attack_weight) / 8;
            if (pos->pieces[enemy_idx][QUEEN] == 0) penalty /= 2;
        }

        int safety = (shield_score - penalty) * phase / 24;
        if (defender == WHITE) ks_score += safety;
        else ks_score -= safety;
    }

    int score = (mg_score * phase + eg_score * (24 - phase)) / 24;
    score += ks_score;
    return score;
}


int run_tune_export(const char *input_path, const char *output_path) {
    FILE *in = fopen(input_path, "r");
    if (!in) { perror("Failed to open input EPD file"); return 1; }
    FILE *out = fopen(output_path, "w");
    if (!out) { perror("Failed to open output CSV file"); fclose(in); return 1; }

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
    printf("Total tunable parameters: %d\n", NUM_PARAMS);

    double *features = malloc(sizeof(double) * NUM_PARAMS);
    if (!features) { perror("Failed to allocate features memory"); fclose(in); fclose(out); return 1; }

    while (fgets(line, sizeof(line), in)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) { line[--len] = '\0'; }
        if (len == 0) continue;

        // EPD Format: <FEN> | <simulated_result> | <white_score>
        char line_copy[1024];
        strcpy(line_copy, line);
        char *fen_part = strtok(line_copy, "|");
        char *result_part = strtok(NULL, "|");
        if (!fen_part || !result_part) continue;

        size_t fen_len = strlen(fen_part);
        while (fen_len > 0 && fen_part[fen_len - 1] == ' ') { fen_part[--fen_len] = '\0'; }
        while (*fen_part == ' ') fen_part++;

        double result_val = atof(result_part);

        Position pos;
        memset(&pos, 0, sizeof(Position));
        if (fen_parse(fen_part, &pos) != 0) continue;

        total_count++;

        int const_score = 0;
        evaluate_extract_features(&pos, features, &const_score);

        // Verification: compare evaluate() with evaluate_replicated()
        int eval_val = evaluate(&pos);
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
        printf("  Discrepancy warnings: %lld (%.2f%%)\n",
               discrepancy_count, (double)discrepancy_count / total_count * 100.0);
    }

    return 0;
}
