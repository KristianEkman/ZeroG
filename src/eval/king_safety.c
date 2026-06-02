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

    // Define the King Zone as the 3x3 grid centered on the king's square
    uint64_t king_zone = kingAttacks[king_sq] | (1ULL << king_sq);

    // Knights
    uint64_t knights = pos->pieces[enemy_side][KNIGHT];
    while (knights) {
        int sq = pop_lsb(&knights);
        if (knightAttacks[sq] & king_zone) {
            attacker_count++;
            attack_weight += KNIGHT_ATTACK_WEIGHT;
        }
    }

    // Bishops
    uint64_t bishops = pos->pieces[enemy_side][BISHOP];
    while (bishops) {
        int sq = pop_lsb(&bishops);
        if (bishopAttacks(sq, occ) & king_zone) {
            attacker_count++;
            attack_weight += BISHOP_ATTACK_WEIGHT;
        }
    }

    // Rooks
    uint64_t rooks = pos->pieces[enemy_side][ROOK];
    while (rooks) {
        int sq = pop_lsb(&rooks);
        if (rookAttacks(sq, occ) & king_zone) {
            attacker_count++;
            attack_weight += ROOK_ATTACK_WEIGHT;
        }
    }

    // Queens
    uint64_t queens = pos->pieces[enemy_side][QUEEN];
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
