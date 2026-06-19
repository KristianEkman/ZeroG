#include "see.h"

/* Simplified piece values used for search ordering and pruning */
static const int see_values[] = {
    0,     /* NONE */
    100,   /* PAWN */
    300,   /* KNIGHT */
    300,   /* BISHOP */
    500,   /* ROOK */
    900,   /* QUEEN */
    20000, /* KING */
    0
};

int see(const Position *pos, Move m) {
    int from = MOVE_FROM(m);
    int to = MOVE_TO(m);
    int flag = MOVE_FLAG(m);

    PieceType attacker = PIECE_TYPE(pos->board[from]);
    if (attacker == NONE) {
        return 0;
    }

    PieceType captured = NONE;
    if (attacker == PAWN && flag == MOVE_QUIET && to == pos->enPassantSquare) {
        captured = PAWN;
    } else {
        captured = PIECE_TYPE(pos->board[to]);
    }

    /* Handle pawn promotions: the attacker effectively becomes the promoted piece,
       and the initial gain includes the value increase of the promotion. */
    PieceType promo_pt = NONE;
    if (attacker == PAWN && (RANK_OF(to) == 0 || RANK_OF(to) == 7)) {
        static const PieceType promo_table[] = { KNIGHT, BISHOP, ROOK, QUEEN };
        int promo_idx = MOVE_PROMO(m);
        promo_pt = (promo_idx <= 3) ? promo_table[promo_idx] : QUEEN;
    }

    int gain[32];
    int depth = 0;

    /* Step 0: we capture the piece currently on 'to' */
    if (promo_pt != NONE) {
        gain[0] = see_values[captured] + (see_values[promo_pt] - see_values[PAWN]);
        attacker = promo_pt; // Subsequent captures will target the promoted piece
    } else {
        gain[0] = see_values[captured];
    }

    /* Set up dynamic occupancy */
    uint64_t occ = pos->occAll;
    occ &= ~(1ULL << from); // Remove capturing piece from its origin
    occ |= (1ULL << to);    // Place it on the destination square

    /* If it was an en-passant capture, remove the captured pawn from its actual square */
    if (flag == MOVE_QUIET && to == pos->enPassantSquare && PIECE_TYPE(pos->board[from]) == PAWN) {
        int cap_sq = (pos->sideToMove == WHITE) ? to - 8 : to + 8;
        occ &= ~(1ULL << cap_sq);
    }

    Color us = OPPOSITE(pos->sideToMove);
    int current_val = see_values[attacker];

    /* Cache constant slider lists and static attackers once at the start */
    uint64_t diagonal_sliders = (pos->pieces[COLOR_IDX(WHITE)][BISHOP] | pos->pieces[COLOR_IDX(BLACK)][BISHOP] |
                                 pos->pieces[COLOR_IDX(WHITE)][QUEEN]  | pos->pieces[COLOR_IDX(BLACK)][QUEEN]);
    uint64_t orthogonal_sliders = (pos->pieces[COLOR_IDX(WHITE)][ROOK]  | pos->pieces[COLOR_IDX(BLACK)][ROOK] |
                                   pos->pieces[COLOR_IDX(WHITE)][QUEEN] | pos->pieces[COLOR_IDX(BLACK)][QUEEN]);

    /* Pawns, knights, and kings attacks are static and only filtered by active occupancy */
    uint64_t static_attackers = 0;
    static_attackers |= pawnAttacks[COLOR_IDX(BLACK)][to] & pos->pieces[COLOR_IDX(WHITE)][PAWN];
    static_attackers |= pawnAttacks[COLOR_IDX(WHITE)][to] & pos->pieces[COLOR_IDX(BLACK)][PAWN];
    static_attackers |= knightAttacks[to] & (pos->pieces[COLOR_IDX(WHITE)][KNIGHT] | pos->pieces[COLOR_IDX(BLACK)][KNIGHT]);
    static_attackers |= kingAttacks[to] & (pos->pieces[COLOR_IDX(WHITE)][KING] | pos->pieces[COLOR_IDX(BLACK)][KING]);

    /* Initial attackers set */
    uint64_t attackers = static_attackers;
    if (bishopEmptyAttacks[to] & diagonal_sliders) {
        attackers |= bishopAttacks(to, occ) & diagonal_sliders;
    }
    if (rookEmptyAttacks[to] & orthogonal_sliders) {
        attackers |= rookAttacks(to, occ) & orthogonal_sliders;
    }

    while (1) {
        /* Filter attackers to just the active side that are still occupied in occ */
        uint64_t active_attackers = attackers & occ & pos->occ[COLOR_IDX(us)];
        if (!active_attackers) {
            break;
        }

        /* Find the cheapest attacker of the active side */
        int next_sq = -1;
        PieceType next_type = NONE;

        for (PieceType pt = PAWN; pt <= KING; pt++) {
            uint64_t subset = active_attackers & pos->pieces[COLOR_IDX(us)][pt];
            if (subset) {
                next_sq = pop_lsb(&subset);
                next_type = pt;
                break;
            }
        }

        if (next_sq == -1) {
            break;
        }

        depth++;
        gain[depth] = current_val;
        current_val = see_values[next_type];

        /* Update occupancy */
        occ &= ~(1ULL << next_sq);

        /* Update only sliding attacks from the new occupancy */
        uint64_t slider_attacks = 0;
        if (bishopEmptyAttacks[to] & diagonal_sliders) {
            slider_attacks |= bishopAttacks(to, occ) & diagonal_sliders;
        }
        if (rookEmptyAttacks[to] & orthogonal_sliders) {
            slider_attacks |= rookAttacks(to, occ) & orthogonal_sliders;
        }
        attackers = static_attackers | slider_attacks;

        /* Swap sides */
        us = OPPOSITE(us);
    }

    /* Backpropagate the minimax scores */
    while (depth > 0) {
        int next_gain = gain[depth];
        if (next_gain < 0) {
            next_gain = 0; // Standing pat is always an option
        }
        gain[depth - 1] = gain[depth - 1] - next_gain;
        depth--;
    }

    return gain[0];
}
