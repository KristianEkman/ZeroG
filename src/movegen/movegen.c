#include "movegen.h"
#include "zobrist.h"
#include "eval.h"
#include "nn.h"
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal: update castling rights after a move
 * ──────────────────────────────────────────────────────────────────────────── */

static uint8_t castling_after_move(uint8_t cr, int from, int to, Color us)
{
    /* Moving side loses rights when its own king/rook moves.  Capture-based
     * rights loss is independent of the moving side: either side can capture
     * a rook on its original square. */
    if (us == WHITE) {
        /* White king moved – lose both white castling rights */
        if (from == E1) cr &= ~(CASTLE_WK | CASTLE_WQ);
        /* Rook moves */
        if (from == A1) cr &= ~CASTLE_WQ;
        if (from == H1) cr &= ~CASTLE_WK;
    } else {
        if (from == E8) cr &= ~(CASTLE_BK | CASTLE_BQ);
        if (from == A8) cr &= ~CASTLE_BQ;
        if (from == H8) cr &= ~CASTLE_BK;
    }

    if (to == A1) cr &= ~CASTLE_WQ;
    if (to == H1) cr &= ~CASTLE_WK;
    if (to == A8) cr &= ~CASTLE_BQ;
    if (to == H8) cr &= ~CASTLE_BK;

    return cr;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * apply_move – make a move, saving undo information
 * ──────────────────────────────────────────────────────────────────────────── */

void apply_move(Position *pos, Move m, Undo *u)
{
    int from    = MOVE_FROM(m);
    int to      = MOVE_TO(m);
    int flag    = MOVE_FLAG(m);
    int promo   = MOVE_PROMO(m);
    Color us    = pos->sideToMove;
    Color them  = OPPOSITE(us);
    int us_idx  = COLOR_IDX(us);
    int them_idx = COLOR_IDX(them);

    Piece moving_piece = pos->board[from];
    PieceType pt = PIECE_TYPE(moving_piece);
    uint64_t from_bb = 1ULL << from;
    uint64_t to_bb   = 1ULL << to;

    /* ── Save undo state ──────────────────────────────────────────────── */
    u->move         = m;
    u->moving       = moving_piece;
    u->old_ep       = pos->enPassantSquare;
    u->old_castling = pos->castlingRights;
    u->old_fifty    = pos->fiftyMoveCounter;
    u->old_hash     = pos->hashKey;
    if (use_nn && eval_nn) {
        memcpy(u->accum, pos->accum, sizeof(pos->accum));
    }

    /* Captured piece (handle EP capture separately) */
    if (pt == PAWN && to == pos->enPassantSquare) {
        u->captured = MAKE_PIECE(them, PAWN);
        u->cap_sq   = (us == WHITE) ? to - 8 : to + 8;
    } else {
        u->captured = pos->board[to];
        u->cap_sq   = to;
    }

    /* ── 1. Remove piece from source square ───────────────────────────── */
    pos->pieces[us_idx][pt] ^= from_bb;
    pos->occ[us_idx]        ^= from_bb;
    pos->board[from]         = EMPTY;
    pos->hashKey            ^= zobrist_piece_key(moving_piece, from);

    /* ── 2. Remove captured piece (non-EP) ────────────────────────────── */
    if (u->captured != EMPTY && !(pt == PAWN && to == pos->enPassantSquare)) {
        PieceType cpt = PIECE_TYPE(u->captured);
        pos->pieces[them_idx][cpt] ^= to_bb;
        pos->occ[them_idx]        ^= to_bb;
        pos->hashKey              ^= zobrist_piece_key(u->captured, to);
    }

    /* ── 3. Castling rook relocation ──────────────────────────────────── */
    if (flag == MOVE_CASTLE_KS) {
        int rook_from = (us == WHITE) ? H1 : H8;
        int rook_to   = (us == WHITE) ? F1 : F8;
        uint64_t rfrom_bb = 1ULL << rook_from;
        uint64_t rto_bb   = 1ULL << rook_to;
        pos->pieces[us_idx][ROOK] ^= rfrom_bb | rto_bb;
        pos->occ[us_idx]          ^= rfrom_bb | rto_bb;
        pos->board[rook_from]      = EMPTY;
        pos->board[rook_to]        = MAKE_PIECE(us, ROOK);
        pos->hashKey              ^= zobrist_piece_key(MAKE_PIECE(us, ROOK), rook_from);
        pos->hashKey              ^= zobrist_piece_key(MAKE_PIECE(us, ROOK), rook_to);
    } else if (flag == MOVE_CASTLE_QS) {
        int rook_from = (us == WHITE) ? A1 : A8;
        int rook_to   = (us == WHITE) ? D1 : D8;
        uint64_t rfrom_bb = 1ULL << rook_from;
        uint64_t rto_bb   = 1ULL << rook_to;
        pos->pieces[us_idx][ROOK] ^= rfrom_bb | rto_bb;
        pos->occ[us_idx]          ^= rfrom_bb | rto_bb;
        pos->board[rook_from]      = EMPTY;
        pos->board[rook_to]        = MAKE_PIECE(us, ROOK);
        pos->hashKey              ^= zobrist_piece_key(MAKE_PIECE(us, ROOK), rook_from);
        pos->hashKey              ^= zobrist_piece_key(MAKE_PIECE(us, ROOK), rook_to);
    }

    /* ── 4. En passant – remove captured pawn behind destination ──────── */
    if (pt == PAWN && to == pos->enPassantSquare) {
        int cap_sq = u->cap_sq;
        uint64_t cap_bb = 1ULL << cap_sq;
        pos->pieces[them_idx][PAWN] ^= cap_bb;
        pos->occ[them_idx]          ^= cap_bb;
        pos->board[cap_sq]           = EMPTY;
        pos->hashKey                ^= zobrist_piece_key(MAKE_PIECE(them, PAWN), cap_sq);
    }

    /* ── 5. Place piece on destination (handle promotion) ─────────────── */
    int is_promo = (pt == PAWN) && (RANK_OF(to) == 0 || RANK_OF(to) == 7);
    if (is_promo) {
        static const PieceType promo_table[] = { KNIGHT, BISHOP, ROOK, QUEEN };
        PieceType promo_pt = (promo <= 3) ? promo_table[promo] : QUEEN;
        pos->pieces[us_idx][promo_pt] ^= to_bb;
        pos->occ[us_idx]              ^= to_bb;
        pos->board[to]                 = MAKE_PIECE(us, promo_pt);
        pos->hashKey                  ^= zobrist_piece_key(MAKE_PIECE(us, promo_pt), to);
    } else {
        pos->pieces[us_idx][pt] ^= to_bb;
        pos->occ[us_idx]        ^= to_bb;
        pos->board[to]           = MAKE_PIECE(us, pt);
        pos->hashKey            ^= zobrist_piece_key(moving_piece, to);
    }

    /* ── 6. Update king square ────────────────────────────────────────── */
    if (pt == KING)
        pos->kingSq[us_idx] = to;

    /* ── 7. Rebuild occupancy ─────────────────────────────────────────── */
    pos->occAll = pos->occ[COLOR_IDX(WHITE)] | pos->occ[COLOR_IDX(BLACK)];

    /* ── 8. Update en passant square ──────────────────────────────────── */
    if (u->old_ep != SQ_NONE) {
        pos->hashKey ^= zobrist_en_passant_key(u->old_ep);
    }
    pos->enPassantSquare = SQ_NONE;
    if (pt == PAWN && (to - from == 16 || from - to == 16)) {
        /* Double push – set ep square to the passed-over square */
        pos->enPassantSquare = (us == WHITE) ? to - 8 : to + 8;
        pos->hashKey         ^= zobrist_en_passant_key(pos->enPassantSquare);
    }

    /* ── 9. Update castling rights ────────────────────────────────────── */
    pos->hashKey        ^= zobrist_castling_key(u->old_castling);
    pos->castlingRights  = castling_after_move(u->old_castling, from, to, us);
    pos->hashKey        ^= zobrist_castling_key(pos->castlingRights);

    /* ── 10. Update fifty-move counter ────────────────────────────────── */
    if (pt == PAWN || u->captured != EMPTY)
        pos->fiftyMoveCounter = 0;
    else
        pos->fiftyMoveCounter++;

    /* ── 11. Increment fullmove number ─────────────────────────────────── */
    if (us == BLACK)
        pos->fullmoveNumber++;

    /* ── 12. Switch side to move ──────────────────────────────────────── */
    pos->sideToMove = them;
    pos->hashKey   ^= zobrist_side_to_move_key();
    
    if (use_nn && eval_nn) {
        nnue_update_accumulator(eval_nn, pos, m, u);
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * undo_move – restore position from undo state
 * ──────────────────────────────────────────────────────────────────────────── */

void undo_move(Position *pos, const Undo *u)
{
    Move m    = u->move;
    int from  = MOVE_FROM(m);
    int to    = MOVE_TO(m);
    int flag  = MOVE_FLAG(m);
    int promo = MOVE_PROMO(m);

    /* sideToMove was flipped by apply_move, so the side that just *moved*
     * is now the opponent of sideToMove.  The side that moved is `us`. */
    Color us   = OPPOSITE(pos->sideToMove);
    Color them = pos->sideToMove;
    int us_idx  = COLOR_IDX(us);
    int them_idx = COLOR_IDX(them);

    Piece moved = u->moving;
    PieceType pt = PIECE_TYPE(moved);
    uint64_t from_bb = 1ULL << from;
    uint64_t to_bb   = 1ULL << to;

    /* ── 1. Remove piece from destination ─────────────────────────────── */
    int is_promo = (pt == PAWN) && (RANK_OF(to) == 0 || RANK_OF(to) == 7);
    if (is_promo) {
        static const PieceType promo_table[] = { KNIGHT, BISHOP, ROOK, QUEEN };
        PieceType promo_pt = (promo <= 3) ? promo_table[promo] : QUEEN;
        pos->pieces[us_idx][promo_pt] ^= to_bb;
        pos->occ[us_idx]              ^= to_bb;
    } else {
        pos->pieces[us_idx][pt] ^= to_bb;
        pos->occ[us_idx]        ^= to_bb;
    }

    /* ── 2. Restore piece to source square ────────────────────────────── */
    pos->pieces[us_idx][pt] ^= from_bb;
    pos->occ[us_idx]        ^= from_bb;
    pos->board[from]         = moved;

    /* The moved piece has been removed from the destination bitboards above;
     * clear the mailbox destination as well.  Captures then restore the
     * captured piece to cap_sq, which differs from `to` for en passant. */
    pos->board[to] = EMPTY;

    /* ── 3. Restore captured piece ────────────────────────────────────── */
    if (u->captured != EMPTY) {
        PieceType cpt = PIECE_TYPE(u->captured);
        uint64_t cap_bb = 1ULL << u->cap_sq;
        pos->pieces[them_idx][cpt] ^= cap_bb;
        pos->occ[them_idx]         ^= cap_bb;
        pos->board[u->cap_sq]       = u->captured;
    }

    /* ── 4. Undo castling rook relocation ─────────────────────────────── */
    if (flag == MOVE_CASTLE_KS) {
        int rook_from = (us == WHITE) ? H1 : H8;
        int rook_to   = (us == WHITE) ? F1 : F8;
        uint64_t rfrom_bb = 1ULL << rook_from;
        uint64_t rto_bb   = 1ULL << rook_to;
        pos->pieces[us_idx][ROOK] ^= rfrom_bb | rto_bb;
        pos->occ[us_idx]          ^= rfrom_bb | rto_bb;
        pos->board[rook_to]        = EMPTY;
        pos->board[rook_from]      = MAKE_PIECE(us, ROOK);
    } else if (flag == MOVE_CASTLE_QS) {
        int rook_from = (us == WHITE) ? A1 : A8;
        int rook_to   = (us == WHITE) ? D1 : D8;
        uint64_t rfrom_bb = 1ULL << rook_from;
        uint64_t rto_bb   = 1ULL << rook_to;
        pos->pieces[us_idx][ROOK] ^= rfrom_bb | rto_bb;
        pos->occ[us_idx]          ^= rfrom_bb | rto_bb;
        pos->board[rook_to]        = EMPTY;
        pos->board[rook_from]      = MAKE_PIECE(us, ROOK);
    }

    /* ── 5. Restore king square ───────────────────────────────────────── */
    if (pt == KING)
        pos->kingSq[us_idx] = from;

    /* ── 6. Restore occupancy ─────────────────────────────────────────── */
    pos->occAll = pos->occ[COLOR_IDX(WHITE)] | pos->occ[COLOR_IDX(BLACK)];

    /* ── 7. Restore state fields ──────────────────────────────────────── */
    pos->enPassantSquare = u->old_ep;
    pos->castlingRights  = u->old_castling;
    pos->fiftyMoveCounter = u->old_fifty;
    pos->hashKey         = u->old_hash;

    /* ── 8. Restore fullmove number ───────────────────────────────────── */
    if (us == BLACK)
        pos->fullmoveNumber--;

    /* ── 9. Restore side to move ──────────────────────────────────────── */
    pos->sideToMove = us;
    
    if (use_nn && eval_nn) {
        memcpy(pos->accum, u->accum, sizeof(pos->accum));
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * is_square_attacked
 * ──────────────────────────────────────────────────────────────────────────── */

int is_square_attacked(const Position *pos, int sq, Color attacker)
{
    int a_idx = COLOR_IDX(attacker);
    uint64_t occ = pos->occAll;

    /* Pawn attacks: a pawn of `attacker` would attack `sq` */
    uint64_t pawns = pos->pieces[a_idx][PAWN];
    if (pawns && (pawnAttacks[COLOR_IDX(OPPOSITE(attacker))][sq] & pawns))
        return 1;

    /* Knight */
    uint64_t knights = pos->pieces[a_idx][KNIGHT];
    if (knights && (knightAttacks[sq] & knights))
        return 1;

    /* Bishop / Queen diagonals */
    uint64_t sliders_diag = pos->pieces[a_idx][BISHOP] | pos->pieces[a_idx][QUEEN];
    if (sliders_diag && (bishopAttacks(sq, occ) & sliders_diag))
        return 1;

    /* Rook / Queen orthogonals */
    uint64_t sliders_orth = pos->pieces[a_idx][ROOK] | pos->pieces[a_idx][QUEEN];
    if (sliders_orth && (rookAttacks(sq, occ) & sliders_orth))
        return 1;

    /* King */
    if (kingAttacks[sq] & pos->pieces[a_idx][KING])
        return 1;

    return 0;
}


/* ─────────────────────────────────────────────────────────────────────────────
 * movegen_pseudo_legal
 * ──────────────────────────────────────────────────────────────────────────── */

int movegen_pseudo_legal(const Position *pos, Move *moves)
{
    Move *m = moves;
    Color us   = pos->sideToMove;
    Color them = OPPOSITE(us);
    int us_idx = COLOR_IDX(us);

    uint64_t own    = pos->occ[us_idx];
    uint64_t enemy  = pos->occ[COLOR_IDX(them)];
    uint64_t occ    = pos->occAll;
    uint64_t empty  = ~occ;

    int ep_sq = pos->enPassantSquare;

    /* ── Pawn moves ───────────────────────────────────────────────────────── */
    {
        uint64_t pawns = pos->pieces[us_idx][PAWN];

        if (us == WHITE) {
            /* Single push: shift north by 8, only to empty squares */
            uint64_t push1 = (pawns << 8) & empty;
            /* Promotions from push1 (rank 8) */
            uint64_t promo_push = push1 & 0xFF00000000000000ULL;
            uint64_t quiet_push = push1 & ~promo_push;

            while (quiet_push) {
                int to = pop_lsb(&quiet_push);
                int from = to - 8;
                *m++ = MOVE_BUILD(from, to, 0, MOVE_QUIET);
            }
            while (promo_push) {
                int to = pop_lsb(&promo_push);
                int from = to - 8;
                for (int pp = 0; pp < 4; pp++)
                    *m++ = MOVE_BUILD(from, to, pp, MOVE_QUIET);
            }

            /* Double push: from rank 2, both intermediate and target must be empty */
            uint64_t dp = (pawns & 0x000000000000FF00ULL) << 16;
            dp &= empty & (empty << 8);
            while (dp) {
                int to = pop_lsb(&dp);
                int from = to - 16;
                *m++ = MOVE_BUILD(from, to, 0, MOVE_DOUBLE_PUSH);
            }

            /* Captures: north-east (+9) from non-H-file, north-west (+7) from non-A-file */
            uint64_t cap_east = (pawns & ~0x8080808080808080ULL) << 9;
            uint64_t cap_west = (pawns & ~0x0101010101010101ULL) << 7;

            /* Normal captures */
            uint64_t cap_e = cap_east & enemy;
            uint64_t cap_w = cap_west & enemy;

            /* Promotion captures */
            uint64_t promo_e = cap_e & 0xFF00000000000000ULL;
            uint64_t promo_w = cap_w & 0xFF00000000000000ULL;
            cap_e &= ~promo_e;
            cap_w &= ~promo_w;

            while (cap_e) { int to = pop_lsb(&cap_e);  *m++ = MOVE_BUILD(to - 9, to, 0, MOVE_QUIET); }
            while (cap_w) { int to = pop_lsb(&cap_w);  *m++ = MOVE_BUILD(to - 7, to, 0, MOVE_QUIET); }
            while (promo_e) { int to = pop_lsb(&promo_e);
                for (int pp = 0; pp < 4; pp++) *m++ = MOVE_BUILD(to - 9, to, pp, MOVE_QUIET); }
            while (promo_w) { int to = pop_lsb(&promo_w);
                for (int pp = 0; pp < 4; pp++) *m++ = MOVE_BUILD(to - 7, to, pp, MOVE_QUIET); }

            /* En passant */
            if (ep_sq != SQ_NONE) {
                uint64_t ep_bb = 1ULL << ep_sq;
                /* Attacking pawns must be on rank 5, on either side of ep square */
                if (FILE_OF(ep_sq) > 0) {
                    uint64_t attacker = ep_bb >> 9;  /* pawn south-east of ep */
                    if (pawns & attacker) *m++ = MOVE_BUILD(ep_sq - 9, ep_sq, 0, MOVE_QUIET);
                }
                if (FILE_OF(ep_sq) < 7) {
                    uint64_t attacker = ep_bb >> 7;  /* pawn south-west of ep */
                    if (pawns & attacker) *m++ = MOVE_BUILD(ep_sq - 7, ep_sq, 0, MOVE_QUIET);
                }
            }

        } else { /* BLACK */
            /* Single push south */
            uint64_t push1 = (pawns >> 8) & empty;
            uint64_t promo_push = push1 & 0x00000000000000FFULL;  /* rank 1 */
            uint64_t quiet_push = push1 & ~promo_push;

            while (quiet_push) {
                int to = pop_lsb(&quiet_push);
                int from = to + 8;
                *m++ = MOVE_BUILD(from, to, 0, MOVE_QUIET);
            }
            while (promo_push) {
                int to = pop_lsb(&promo_push);
                int from = to + 8;
                for (int pp = 0; pp < 4; pp++)
                    *m++ = MOVE_BUILD(from, to, pp, MOVE_QUIET);
            }

            /* Double push: from rank 7 */
            uint64_t dp = (pawns & 0x00FF000000000000ULL) >> 16;
            dp &= empty & (empty >> 8);
            while (dp) {
                int to = pop_lsb(&dp);
                int from = to + 16;
                *m++ = MOVE_BUILD(from, to, 0, MOVE_DOUBLE_PUSH);
            }

            /* Captures: south-east (-7) from non-H-file, south-west (-9) from non-A-file */
            uint64_t cap_east = (pawns & ~0x8080808080808080ULL) >> 7;
            uint64_t cap_west = (pawns & ~0x0101010101010101ULL) >> 9;

            uint64_t cap_e = cap_east & enemy;
            uint64_t cap_w = cap_west & enemy;

            uint64_t promo_e = cap_e & 0x00000000000000FFULL;
            uint64_t promo_w = cap_w & 0x00000000000000FFULL;
            cap_e &= ~promo_e;
            cap_w &= ~promo_w;

            while (cap_e) { int to = pop_lsb(&cap_e);  *m++ = MOVE_BUILD(to + 7, to, 0, MOVE_QUIET); }
            while (cap_w) { int to = pop_lsb(&cap_w);  *m++ = MOVE_BUILD(to + 9, to, 0, MOVE_QUIET); }
            while (promo_e) { int to = pop_lsb(&promo_e);
                for (int pp = 0; pp < 4; pp++) *m++ = MOVE_BUILD(to + 7, to, pp, MOVE_QUIET); }
            while (promo_w) { int to = pop_lsb(&promo_w);
                for (int pp = 0; pp < 4; pp++) *m++ = MOVE_BUILD(to + 9, to, pp, MOVE_QUIET); }

            /* En passant */
            if (ep_sq != SQ_NONE) {
                uint64_t ep_bb = 1ULL << ep_sq;
                if (FILE_OF(ep_sq) > 0) {
                    uint64_t attacker = ep_bb << 7;  /* pawn north-east of ep */
                    if (pawns & attacker) *m++ = MOVE_BUILD(ep_sq + 7, ep_sq, 0, MOVE_QUIET);
                }
                if (FILE_OF(ep_sq) < 7) {
                    uint64_t attacker = ep_bb << 9;  /* pawn north-west of ep */
                    if (pawns & attacker) *m++ = MOVE_BUILD(ep_sq + 9, ep_sq, 0, MOVE_QUIET);
                }
            }
        }
    }

    /* ── Knight moves ─────────────────────────────────────────────────────── */
    {
        uint64_t knights = pos->pieces[us_idx][KNIGHT];
        while (knights) {
            int from = pop_lsb(&knights);
            uint64_t targets = knightAttacks[from] & ~own;
            while (targets) {
                int to = pop_lsb(&targets);
                *m++ = MOVE_BUILD(from, to, 0, MOVE_QUIET);
            }
        }
    }

    /* ── Bishop moves ─────────────────────────────────────────────────────── */
    {
        uint64_t bishops = pos->pieces[us_idx][BISHOP];
        while (bishops) {
            int from = pop_lsb(&bishops);
            uint64_t targets = bishopAttacks(from, occ) & ~own;
            while (targets) {
                int to = pop_lsb(&targets);
                *m++ = MOVE_BUILD(from, to, 0, MOVE_QUIET);
            }
        }
    }

    /* ── Rook moves ───────────────────────────────────────────────────────── */
    {
        uint64_t rooks = pos->pieces[us_idx][ROOK];
        while (rooks) {
            int from = pop_lsb(&rooks);
            uint64_t targets = rookAttacks(from, occ) & ~own;
            while (targets) {
                int to = pop_lsb(&targets);
                *m++ = MOVE_BUILD(from, to, 0, MOVE_QUIET);
            }
        }
    }

    /* ── Queen moves ──────────────────────────────────────────────────────── */
    {
        uint64_t queens = pos->pieces[us_idx][QUEEN];
        while (queens) {
            int from = pop_lsb(&queens);
            uint64_t targets = queenAttacks(from, occ) & ~own;
            while (targets) {
                int to = pop_lsb(&targets);
                *m++ = MOVE_BUILD(from, to, 0, MOVE_QUIET);
            }
        }
    }

    /* ── King moves (including castling) ──────────────────────────────────── */
    {
        int from = pos->kingSq[us_idx];
        uint64_t targets = kingAttacks[from] & ~own;
        while (targets) {
            int to = pop_lsb(&targets);
            *m++ = MOVE_BUILD(from, to, 0, MOVE_QUIET);
        }

        /* Castling */
        uint8_t cr = pos->castlingRights;
        if (us == WHITE) {
            if ((cr & CASTLE_WK)
                && !(occ & 0x0000000000000060ULL)
                && pos->board[E1] == W_KING
                && pos->board[H1] == W_ROOK)
            {
                *m++ = MOVE_BUILD(E1, G1, 0, MOVE_CASTLE_KS);
            }
            if ((cr & CASTLE_WQ)
                && !(occ & 0x000000000000000EULL)
                && pos->board[E1] == W_KING
                && pos->board[A1] == W_ROOK)
            {
                *m++ = MOVE_BUILD(E1, C1, 0, MOVE_CASTLE_QS);
            }
        } else {
            if ((cr & CASTLE_BK)
                && !(occ & 0x6000000000000000ULL)
                && pos->board[E8] == B_KING
                && pos->board[H8] == B_ROOK)
            {
                *m++ = MOVE_BUILD(E8, G8, 0, MOVE_CASTLE_KS);
            }
            if ((cr & CASTLE_BQ)
                && !(occ & 0x0E00000000000000ULL)
                && pos->board[E8] == B_KING
                && pos->board[A8] == B_ROOK)
            {
                *m++ = MOVE_BUILD(E8, C8, 0, MOVE_CASTLE_QS);
            }
        }
    }

    return (int)(m - moves);
}


/* ─────────────────────────────────────────────────────────────────────────────
 * movegen_legal
 * ──────────────────────────────────────────────────────────────────────────── */

int movegen_legal(const Position *pos, Move *moves)
{
    Move pseudo[MAX_MOVES];
    int n = movegen_pseudo_legal(pos, pseudo);
    Move *m = moves;

    Color us = pos->sideToMove;
    int king_sq = pos->kingSq[COLOR_IDX(us)];

    for (int i = 0; i < n; i++) {
        Move mv = pseudo[i];

        /* Quick filter for castling: king must not be in check and must not
         * move through or into check */
        if (MOVE_FLAG(mv) == MOVE_CASTLE_KS) {
            if (is_square_attacked(pos, king_sq, OPPOSITE(us)))
                continue;
            int step_sq = (us == WHITE) ? F1 : F8;
            if (is_square_attacked(pos, step_sq, OPPOSITE(us)))
                continue;
            int dest = (us == WHITE) ? G1 : G8;
            if (is_square_attacked(pos, dest, OPPOSITE(us)))
                continue;
        }
        if (MOVE_FLAG(mv) == MOVE_CASTLE_QS) {
            if (is_square_attacked(pos, king_sq, OPPOSITE(us)))
                continue;
            int step_sq = (us == WHITE) ? D1 : D8;
            if (is_square_attacked(pos, step_sq, OPPOSITE(us)))
                continue;
            int dest = (us == WHITE) ? C1 : C8;
            if (is_square_attacked(pos, dest, OPPOSITE(us)))
                continue;
        }

        /* Apply the move using make/unmake and test king safety */
        Undo u;
        Position *mut = (Position *)pos;  /* safe: we undo before returning */
        apply_move(mut, mv, &u);

        int new_king_sq = mut->kingSq[COLOR_IDX(us)];

        if (!is_square_attacked(mut, new_king_sq, OPPOSITE(us)))
            *m++ = mv;

        undo_move(mut, &u);
    }

    return (int)(m - moves);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * perft – performance test: count legal leaf nodes at the given depth
 * ──────────────────────────────────────────────────────────────────────────── */

uint64_t perft(Position *pos, int depth)
{
    if (depth == 0)
        return 1ULL;

    Move moves[MAX_MOVES];
    int n = movegen_legal(pos, moves);

    if (depth == 1)
        return (uint64_t)n;

    uint64_t nodes = 0;
    for (int i = 0; i < n; i++) {
        Undo u;
        apply_move(pos, moves[i], &u);
        nodes += perft(pos, depth - 1);
        undo_move(pos, &u);
    }
    return nodes;
}