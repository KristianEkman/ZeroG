#include "eval_mobility.h"
#include "eval_constants.h"

/* PeSTO-style mobility weights (MiddleGame and EndGame) */
static const int KnightMobilityMG[9] = {
    KNIGHT_MOBILITY_MG_0_VAL, KNIGHT_MOBILITY_MG_1_VAL, KNIGHT_MOBILITY_MG_2_VAL,
    KNIGHT_MOBILITY_MG_3_VAL, KNIGHT_MOBILITY_MG_4_VAL, KNIGHT_MOBILITY_MG_5_VAL,
    KNIGHT_MOBILITY_MG_6_VAL, KNIGHT_MOBILITY_MG_7_VAL, KNIGHT_MOBILITY_MG_8_VAL
};
static const int KnightMobilityEG[9] = {
    KNIGHT_MOBILITY_EG_0_VAL, KNIGHT_MOBILITY_EG_1_VAL, KNIGHT_MOBILITY_EG_2_VAL,
    KNIGHT_MOBILITY_EG_3_VAL, KNIGHT_MOBILITY_EG_4_VAL, KNIGHT_MOBILITY_EG_5_VAL,
    KNIGHT_MOBILITY_EG_6_VAL, KNIGHT_MOBILITY_EG_7_VAL, KNIGHT_MOBILITY_EG_8_VAL
};

static const int BishopMobilityMG[14] = {
    BISHOP_MOBILITY_MG_0_VAL, BISHOP_MOBILITY_MG_1_VAL, BISHOP_MOBILITY_MG_2_VAL,
    BISHOP_MOBILITY_MG_3_VAL, BISHOP_MOBILITY_MG_4_VAL, BISHOP_MOBILITY_MG_5_VAL,
    BISHOP_MOBILITY_MG_6_VAL, BISHOP_MOBILITY_MG_7_VAL, BISHOP_MOBILITY_MG_8_VAL,
    BISHOP_MOBILITY_MG_9_VAL, BISHOP_MOBILITY_MG_10_VAL, BISHOP_MOBILITY_MG_11_VAL,
    BISHOP_MOBILITY_MG_12_VAL, BISHOP_MOBILITY_MG_13_VAL
};
static const int BishopMobilityEG[14] = {
    BISHOP_MOBILITY_EG_0_VAL, BISHOP_MOBILITY_EG_1_VAL, BISHOP_MOBILITY_EG_2_VAL,
    BISHOP_MOBILITY_EG_3_VAL, BISHOP_MOBILITY_EG_4_VAL, BISHOP_MOBILITY_EG_5_VAL,
    BISHOP_MOBILITY_EG_6_VAL, BISHOP_MOBILITY_EG_7_VAL, BISHOP_MOBILITY_EG_8_VAL,
    BISHOP_MOBILITY_EG_9_VAL, BISHOP_MOBILITY_EG_10_VAL, BISHOP_MOBILITY_EG_11_VAL,
    BISHOP_MOBILITY_EG_12_VAL, BISHOP_MOBILITY_EG_13_VAL
};

static const int RookMobilityMG[15] = {
    ROOK_MOBILITY_MG_0_VAL, ROOK_MOBILITY_MG_1_VAL, ROOK_MOBILITY_MG_2_VAL,
    ROOK_MOBILITY_MG_3_VAL, ROOK_MOBILITY_MG_4_VAL, ROOK_MOBILITY_MG_5_VAL,
    ROOK_MOBILITY_MG_6_VAL, ROOK_MOBILITY_MG_7_VAL, ROOK_MOBILITY_MG_8_VAL,
    ROOK_MOBILITY_MG_9_VAL, ROOK_MOBILITY_MG_10_VAL, ROOK_MOBILITY_MG_11_VAL,
    ROOK_MOBILITY_MG_12_VAL, ROOK_MOBILITY_MG_13_VAL, ROOK_MOBILITY_MG_14_VAL
};
static const int RookMobilityEG[15] = {
    ROOK_MOBILITY_EG_0_VAL, ROOK_MOBILITY_EG_1_VAL, ROOK_MOBILITY_EG_2_VAL,
    ROOK_MOBILITY_EG_3_VAL, ROOK_MOBILITY_EG_4_VAL, ROOK_MOBILITY_EG_5_VAL,
    ROOK_MOBILITY_EG_6_VAL, ROOK_MOBILITY_EG_7_VAL, ROOK_MOBILITY_EG_8_VAL,
    ROOK_MOBILITY_EG_9_VAL, ROOK_MOBILITY_EG_10_VAL, ROOK_MOBILITY_EG_11_VAL,
    ROOK_MOBILITY_EG_12_VAL, ROOK_MOBILITY_EG_13_VAL, ROOK_MOBILITY_EG_14_VAL
};

static const int QueenMobilityMG[28] = {
    QUEEN_MOBILITY_MG_0_VAL, QUEEN_MOBILITY_MG_1_VAL, QUEEN_MOBILITY_MG_2_VAL,
    QUEEN_MOBILITY_MG_3_VAL, QUEEN_MOBILITY_MG_4_VAL, QUEEN_MOBILITY_MG_5_VAL,
    QUEEN_MOBILITY_MG_6_VAL, QUEEN_MOBILITY_MG_7_VAL, QUEEN_MOBILITY_MG_8_VAL,
    QUEEN_MOBILITY_MG_9_VAL, QUEEN_MOBILITY_MG_10_VAL, QUEEN_MOBILITY_MG_11_VAL,
    QUEEN_MOBILITY_MG_12_VAL, QUEEN_MOBILITY_MG_13_VAL, QUEEN_MOBILITY_MG_14_VAL,
    QUEEN_MOBILITY_MG_15_VAL, QUEEN_MOBILITY_MG_16_VAL, QUEEN_MOBILITY_MG_17_VAL,
    QUEEN_MOBILITY_MG_18_VAL, QUEEN_MOBILITY_MG_19_VAL, QUEEN_MOBILITY_MG_20_VAL,
    QUEEN_MOBILITY_EG_21_VAL, QUEEN_MOBILITY_MG_22_VAL, QUEEN_MOBILITY_MG_23_VAL,
    QUEEN_MOBILITY_MG_24_VAL, QUEEN_MOBILITY_MG_25_VAL, QUEEN_MOBILITY_MG_26_VAL,
    QUEEN_MOBILITY_MG_27_VAL
};
static const int QueenMobilityEG[28] = {
    QUEEN_MOBILITY_EG_0_VAL, QUEEN_MOBILITY_EG_1_VAL, QUEEN_MOBILITY_EG_2_VAL,
    QUEEN_MOBILITY_EG_3_VAL, QUEEN_MOBILITY_EG_4_VAL, QUEEN_MOBILITY_EG_5_VAL,
    QUEEN_MOBILITY_EG_6_VAL, QUEEN_MOBILITY_EG_7_VAL, QUEEN_MOBILITY_EG_8_VAL,
    QUEEN_MOBILITY_EG_9_VAL, QUEEN_MOBILITY_EG_10_VAL, QUEEN_MOBILITY_EG_11_VAL,
    QUEEN_MOBILITY_EG_12_VAL, QUEEN_MOBILITY_EG_13_VAL, QUEEN_MOBILITY_EG_14_VAL,
    QUEEN_MOBILITY_EG_15_VAL, QUEEN_MOBILITY_EG_16_VAL, QUEEN_MOBILITY_EG_17_VAL,
    QUEEN_MOBILITY_EG_18_VAL, QUEEN_MOBILITY_EG_19_VAL, QUEEN_MOBILITY_EG_20_VAL,
    QUEEN_MOBILITY_EG_21_VAL, QUEEN_MOBILITY_EG_22_VAL, QUEEN_MOBILITY_EG_23_VAL,
    QUEEN_MOBILITY_EG_24_VAL, QUEEN_MOBILITY_EG_25_VAL, QUEEN_MOBILITY_EG_26_VAL,
    QUEEN_MOBILITY_EG_27_VAL
};

/*
 * Evaluates the mobility for a single color.
 */
static inline int evaluate_color_mobility(const Position *pos, Color us, int phase) {
    Color them = OPPOSITE(us);
    uint64_t own_pieces = pos->occ[COLOR_IDX(us)];
    uint64_t enemy_pawns = pos->pieces[COLOR_IDX(them)][PAWN];
    
    /* Squares attacked by enemy pawns are excluded from our mobility area */
    uint64_t enemy_pawn_attacks;
    if (us == WHITE) {
        enemy_pawn_attacks = ((enemy_pawns & ~0x8080808080808080ULL) >> 7) | 
                             ((enemy_pawns & ~0x0101010101010101ULL) >> 9);
    } else {
        enemy_pawn_attacks = ((enemy_pawns & ~0x8080808080808080ULL) << 9) | 
                             ((enemy_pawns & ~0x0101010101010101ULL) << 7);
    }
    
    uint64_t mobility_area = ~own_pieces & ~enemy_pawn_attacks;
    uint64_t occ = pos->occAll;
    int score_mg = 0;
    int score_eg = 0;
    
    /* Knights */
    uint64_t knights = pos->pieces[COLOR_IDX(us)][KNIGHT];
    while (knights) {
        int sq = pop_lsb(&knights);
        uint64_t attacks = knightAttacks[sq] & mobility_area;
        int count = bit_count(attacks);
        if (count > 8) count = 8;
        score_mg += KnightMobilityMG[count];
        score_eg += KnightMobilityEG[count];
    }
    
    /* Bishops */
    uint64_t bishops = pos->pieces[COLOR_IDX(us)][BISHOP];
    while (bishops) {
        int sq = pop_lsb(&bishops);
        uint64_t attacks = bishopAttacks(sq, occ) & mobility_area;
        int count = bit_count(attacks);
        if (count > 13) count = 13;
        score_mg += BishopMobilityMG[count];
        score_eg += BishopMobilityEG[count];
    }
    
    /* Rooks */
    uint64_t rooks = pos->pieces[COLOR_IDX(us)][ROOK];
    while (rooks) {
        int sq = pop_lsb(&rooks);
        uint64_t attacks = rookAttacks(sq, occ) & mobility_area;
        int count = bit_count(attacks);
        if (count > 14) count = 14;
        score_mg += RookMobilityMG[count];
        score_eg += RookMobilityEG[count];
    }
    
    /* Queens */
    uint64_t queens = pos->pieces[COLOR_IDX(us)][QUEEN];
    while (queens) {
        int sq = pop_lsb(&queens);
        uint64_t attacks = queenAttacks(sq, occ) & mobility_area;
        int count = bit_count(attacks);
        if (count > 27) count = 27;
        score_mg += QueenMobilityMG[count];
        score_eg += QueenMobilityEG[count];
    }
    
    return (score_mg * phase + score_eg * (24 - phase)) / 24;
}

int evaluate_mobility(const Position *pos) {
    int phase = get_game_phase(pos);
    if (phase > 24) phase = 24;
    
    int white_mob = evaluate_color_mobility(pos, WHITE, phase);
    int black_mob = evaluate_color_mobility(pos, BLACK, phase);
    
    return white_mob - black_mob;
}
