#include "eval_mobility.h"

/* PeSTO-style mobility weights (MiddleGame and EndGame) */
static const int KnightMobilityMG[9] = { -20, -10, -5, 0, 5, 10, 15, 20, 22 };
static const int KnightMobilityEG[9] = { -20, -10, -5, 0, 5, 10, 15, 20, 22 };

static const int BishopMobilityMG[14] = { -20, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50 };
static const int BishopMobilityEG[14] = { -20, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50 };

static const int RookMobilityMG[15] = { -20, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55 };
static const int RookMobilityEG[15] = { -25, -15, -5, 0, 10, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65 };

static const int QueenMobilityMG[28] = { 
    -10, -5, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 
    24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50 
};
static const int QueenMobilityEG[28] = { 
    -20, -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 
    50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115 
};

/* 
 * Calculate game phase based on remaining minor/major pieces.
 * Starts at 24 (opening/middlegame) and decreases to 0 (endgame).
 */
static inline int get_game_phase(const Position *pos) {
    int knights = bit_count(pos->pieces[0][KNIGHT] | pos->pieces[1][KNIGHT]);
    int bishops = bit_count(pos->pieces[0][BISHOP] | pos->pieces[1][BISHOP]);
    int rooks   = bit_count(pos->pieces[0][ROOK]   | pos->pieces[1][ROOK]);
    int queens  = bit_count(pos->pieces[0][QUEEN]  | pos->pieces[1][QUEEN]);
    
    return knights * 1 + bishops * 1 + rooks * 2 + queens * 4;
}

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
