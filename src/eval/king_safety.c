#include "king_safety.h"
#include <stdint.h>

// Pawn Shield Constants
#define PAWN_SHIELD_RANK2_BONUS 10
#define PAWN_SHIELD_RANK3_BONUS 3
#define PAWN_SHIELD_MISSING_PENALTY -15

// King Attack Weights
#define KNIGHT_ATTACK_WEIGHT 2
#define BISHOP_ATTACK_WEIGHT 2
#define ROOK_ATTACK_WEIGHT 3
#define QUEEN_ATTACK_WEIGHT 5

// Non-linear scaling table for attacker counts 0..15
static const int safety_table[16] = {
    0,   0,   20,  50,  90,  140, 200, 270,
    350, 440, 540, 650, 770, 900, 1040, 1190
};

static uint64_t knight_attacks_to_king_zone[64];
static uint64_t bishop_empty_attacks_to_king_zone[64];
static uint64_t rook_empty_attacks_to_king_zone[64];

void init_king_safety_tables(void) {
    for (int sq = 0; sq < 64; sq++) {
        uint64_t king_zone = kingAttacks[sq] | (1ULL << sq);
        
        knight_attacks_to_king_zone[sq] = 0ULL;
        bishop_empty_attacks_to_king_zone[sq] = 0ULL;
        rook_empty_attacks_to_king_zone[sq] = 0ULL;
        
        uint64_t temp = king_zone;
        while (temp) {
            int kz_sq = pop_lsb(&temp);
            knight_attacks_to_king_zone[sq] |= knightAttacks[kz_sq];
            bishop_empty_attacks_to_king_zone[sq] |= bishopEmptyAttacks[kz_sq];
            rook_empty_attacks_to_king_zone[sq] |= rookEmptyAttacks[kz_sq];
        }
    }
}

int evaluate_king_safety(const Position *pos, Color color, int is_endgame) {
    if (is_endgame) {
        return 0;
    }

    int side = COLOR_IDX(color);
    Color them = OPPOSITE(color);
    int enemy_side = COLOR_IDX(them);

    int king_sq = pos->kingSq[side];
    int king_file = FILE_OF(king_sq);
    int king_rank = RANK_OF(king_sq);

    // 1. Pawn Shield Evaluation
    int shield_score = 0;
    int relative_king_rank = (color == WHITE) ? king_rank : (7 - king_rank);

    // Only evaluate pawn shield if king is on ranks 1-3 relative to the player
    if (relative_king_rank <= 2) {
        int start_file, end_file;
        if (king_file >= FILE_F) {
            start_file = FILE_F;
            end_file = FILE_H;
        } else if (king_file <= FILE_C) {
            start_file = FILE_A;
            end_file = FILE_C;
        } else {
            // King is in the center, no shield bonus/penalty
            start_file = -1;
            end_file = -1;
        }

        if (start_file != -1) {
            uint64_t pawns = pos->pieces[side][PAWN];
            int r2 = (color == WHITE) ? RANK_2 : RANK_7;
            int r3 = (color == WHITE) ? RANK_3 : RANK_6;

            for (int f = start_file; f <= end_file; ++f) {
                uint64_t sq_r2 = SQUARE(f, r2);
                uint64_t sq_r3 = SQUARE(f, r3);

                if (pawns & (1ULL << sq_r2)) {
                    shield_score += PAWN_SHIELD_RANK2_BONUS;
                } else if (pawns & (1ULL << sq_r3)) {
                    shield_score += PAWN_SHIELD_RANK3_BONUS;
                } else {
                    shield_score += PAWN_SHIELD_MISSING_PENALTY;
                }
            }
        }
    }

    // 2. King Attacks (Virtual Check / Attacker count)
    int attacker_count = 0;
    int attack_weight = 0;
    uint64_t occ = pos->occAll;

    // Knights (completely loop-free/branch-free)
    int knight_attackers = bit_count(pos->pieces[enemy_side][KNIGHT] & knight_attacks_to_king_zone[king_sq]);
    attacker_count += knight_attackers;
    attack_weight += knight_attackers * KNIGHT_ATTACK_WEIGHT;

    // Define the King Zone as the 3x3 grid centered on the king's square
    uint64_t king_zone = kingAttacks[king_sq] | (1ULL << king_sq);

    // Bishops (filtered by line of sight to king zone)
    uint64_t bishops = pos->pieces[enemy_side][BISHOP] & bishop_empty_attacks_to_king_zone[king_sq];
    while (bishops) {
        int sq = pop_lsb(&bishops);
        if (bishopAttacks(sq, occ) & king_zone) {
            attacker_count++;
            attack_weight += BISHOP_ATTACK_WEIGHT;
        }
    }

    // Rooks (filtered by line of sight to king zone)
    uint64_t rooks = pos->pieces[enemy_side][ROOK] & rook_empty_attacks_to_king_zone[king_sq];
    while (rooks) {
        int sq = pop_lsb(&rooks);
        if (rookAttacks(sq, occ) & king_zone) {
            attacker_count++;
            attack_weight += ROOK_ATTACK_WEIGHT;
        }
    }

    // Queens (filtered by line of sight to king zone)
    uint64_t queens = pos->pieces[enemy_side][QUEEN] & (bishop_empty_attacks_to_king_zone[king_sq] | rook_empty_attacks_to_king_zone[king_sq]);
    while (queens) {
        int sq = pop_lsb(&queens);
        if (queenAttacks(sq, occ) & king_zone) {
            attacker_count++;
            attack_weight += QUEEN_ATTACK_WEIGHT;
        }
    }

    int penalty = 0;
    if (attacker_count >= 2) {
        if (attacker_count > 15) {
            attacker_count = 15;
        }
        penalty = (safety_table[attacker_count] * attack_weight) / 8;

        // If the attacking side has no Queen, the danger is significantly reduced
        if (pos->pieces[enemy_side][QUEEN] == 0) {
            penalty /= 2;
        }
    }

    return shield_score - penalty;
}
