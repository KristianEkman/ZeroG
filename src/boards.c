#include "boards.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Attack tables
 * ──────────────────────────────────────────────────────────────────────────── */
uint64_t knightAttacks[64];
uint64_t kingAttacks[64];
uint64_t pawnAttacks[2][64];

/* ─────────────────────────────────────────────────────────────────────────────
 * Magic bitboards – rook
 * ──────────────────────────────────────────────────────────────────────────── */

/* Offsets for rook occupancies on each square */
static const int rook_shift[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};

/* Rook magics (found with trial and error / pre-generated) */
static const uint64_t rook_magics[64] = {
    0x0080001020400080ULL, 0x0040001000200040ULL, 0x0080081000200080ULL, 0x0080040800100080ULL,
    0x0080020400080080ULL, 0x0080010200040080ULL, 0x0080008001000200ULL, 0x0080002040800100ULL,
    0x0000800020400080ULL, 0x0000400020005000ULL, 0x0000801000200080ULL, 0x0000800800100080ULL,
    0x0000800400080080ULL, 0x0000800200040080ULL, 0x0000800100020080ULL, 0x0000800040801000ULL,
    0x0000200040400080ULL, 0x0000200020005080ULL, 0x0000200010002080ULL, 0x0000200008001080ULL,
    0x0000200004000880ULL, 0x0000200002000480ULL, 0x0000200001000280ULL, 0x0000200000809000ULL,
    0x0000100040400080ULL, 0x0000100020005080ULL, 0x0000100010002080ULL, 0x0000100008001080ULL,
    0x0000100004000880ULL, 0x0000100002000480ULL, 0x0000100001000280ULL, 0x0000100000809000ULL,
    0x0000080040400080ULL, 0x0000080020005080ULL, 0x0000080010002080ULL, 0x0000080008001080ULL,
    0x0000080004000880ULL, 0x0000080002000480ULL, 0x0000080001000280ULL, 0x0000080000809000ULL,
    0x0000040040400080ULL, 0x0000040020005080ULL, 0x0000040010002080ULL, 0x0000040008001080ULL,
    0x0000040004000880ULL, 0x0000040002000480ULL, 0x0000040001000280ULL, 0x0000040000809000ULL,
    0x0080020040400080ULL, 0x0080020020005080ULL, 0x0080020010002080ULL, 0x0080020008001080ULL,
    0x0080020004000880ULL, 0x0080020002000480ULL, 0x0080020001000280ULL, 0x0080020000809000ULL,
    0x0040020040400080ULL, 0x0040020020005080ULL, 0x0040020010002080ULL, 0x0040020008001080ULL,
    0x0040020004000880ULL, 0x0040020002000480ULL, 0x0040020001000280ULL, 0x0040020000809000ULL,
};

/* Rook attack tables: one variable-sized block per square */
static uint64_t *rook_table[64];

/* ─────────────────────────────────────────────────────────────────────────────
 * Magic bitboards – bishop
 * ──────────────────────────────────────────────────────────────────────────── */

static const int bishop_shift[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
};

static const uint64_t bishop_magics[64] = {
    0x0002020202020200ULL, 0x0002020202020000ULL, 0x0004010202000000ULL, 0x0004040080000000ULL,
    0x0001104000000000ULL, 0x0000821040000000ULL, 0x0000410410400000ULL, 0x0000104104104000ULL,
    0x0000040404040400ULL, 0x0000020202020200ULL, 0x0000040102020000ULL, 0x0000040400800000ULL,
    0x0000011040000000ULL, 0x0000008210400000ULL, 0x0000004104104000ULL, 0x0000002082082000ULL,
    0x0004000808080800ULL, 0x0002000404040400ULL, 0x0001000202020200ULL, 0x0000800802004000ULL,
    0x0000800400A00000ULL, 0x0000200100884000ULL, 0x0000400082082000ULL, 0x0000200041041000ULL,
    0x0000080010101000ULL, 0x0000040008080800ULL, 0x0000020004040400ULL, 0x0000010002020200ULL,
    0x0000008002004000ULL, 0x000000800400A000ULL, 0x0000002001008840ULL, 0x0000004000820820ULL,
    0x0000000800101010ULL, 0x0000000400080808ULL, 0x0000000200040404ULL, 0x0000000100020202ULL,
    0x0000000080020040ULL, 0x00000000800400A0ULL, 0x0000000020010088ULL, 0x0000000010008208ULL,
    0x0000000080801010ULL, 0x0000000040400808ULL, 0x0000000020200404ULL, 0x0000000010100202ULL,
    0x0000000008800200ULL, 0x0000000004400400ULL, 0x0000000002200800ULL, 0x0000000001101000ULL,
    0x0000000002020202ULL, 0x0000000002020200ULL, 0x0000000001020200ULL, 0x0000000001008080ULL,
    0x0000000000810400ULL, 0x0000000000410400ULL, 0x0000000000208200ULL, 0x0000000000104100ULL,
    0x0000000002020200ULL, 0x0000000002020000ULL, 0x0000000001020000ULL, 0x0000000001008000ULL,
    0x0000000000810000ULL, 0x0000000000410000ULL, 0x0000000000208000ULL, 0x0000000000104000ULL,
};

static uint64_t *bishop_table[64];

/* ─────────────────────────────────────────────────────────────────────────────
 * Sliding attack masks (without magic)
 * ──────────────────────────────────────────────────────────────────────────── */

static uint64_t rook_mask(int sq) {
    uint64_t mask = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);
    /* north */
    for (int rr = r + 1; rr <= 6; rr++) mask |= (1ULL << SQUARE(f, rr));
    /* south */
    for (int rr = r - 1; rr >= 1; rr--) mask |= (1ULL << SQUARE(f, rr));
    /* east */
    for (int ff = f + 1; ff <= 6; ff++) mask |= (1ULL << SQUARE(ff, r));
    /* west */
    for (int ff = f - 1; ff >= 1; ff--) mask |= (1ULL << SQUARE(ff, r));
    return mask;
}

static uint64_t bishop_mask(int sq) {
    uint64_t mask = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);
    /* north-east */
    for (int rr = r + 1, ff = f + 1; rr <= 6 && ff <= 6; rr++, ff++) mask |= (1ULL << SQUARE(ff, rr));
    /* north-west */
    for (int rr = r + 1, ff = f - 1; rr <= 6 && ff >= 1; rr++, ff--) mask |= (1ULL << SQUARE(ff, rr));
    /* south-east */
    for (int rr = r - 1, ff = f + 1; rr >= 1 && ff <= 6; rr--, ff++) mask |= (1ULL << SQUARE(ff, rr));
    /* south-west */
    for (int rr = r - 1, ff = f - 1; rr >= 1 && ff >= 1; rr--, ff--) mask |= (1ULL << SQUARE(ff, rr));
    return mask;
}

/* Generate sliding attacks naively (given full occupancy) */
static uint64_t rook_attacks_raw(int sq, uint64_t occ) {
    uint64_t att = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);
    /* north */
    for (int rr = r + 1; rr <= 7; rr++) {
        uint64_t bb = 1ULL << SQUARE(f, rr);
        att |= bb;
        if (occ & bb) break;
    }
    /* south */
    for (int rr = r - 1; rr >= 0; rr--) {
        uint64_t bb = 1ULL << SQUARE(f, rr);
        att |= bb;
        if (occ & bb) break;
    }
    /* east */
    for (int ff = f + 1; ff <= 7; ff++) {
        uint64_t bb = 1ULL << SQUARE(ff, r);
        att |= bb;
        if (occ & bb) break;
    }
    /* west */
    for (int ff = f - 1; ff >= 0; ff--) {
        uint64_t bb = 1ULL << SQUARE(ff, r);
        att |= bb;
        if (occ & bb) break;
    }
    return att;
}

static uint64_t bishop_attacks_raw(int sq, uint64_t occ) {
    uint64_t att = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);
    /* north-east */
    for (int rr = r + 1, ff = f + 1; rr <= 7 && ff <= 7; rr++, ff++) {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb) break;
    }
    /* north-west */
    for (int rr = r + 1, ff = f - 1; rr <= 7 && ff >= 0; rr++, ff--) {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb) break;
    }
    /* south-east */
    for (int rr = r - 1, ff = f + 1; rr >= 0 && ff <= 7; rr--, ff++) {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb) break;
    }
    /* south-west */
    for (int rr = r - 1, ff = f - 1; rr >= 0 && ff >= 0; rr--, ff--) {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb) break;
    }
    return att;
}

/* Generate all occupancy permutations from a mask */
static uint64_t set_occupancy(int index, int bits, uint64_t mask) {
    uint64_t occ = 0ULL;
    for (int count = 0; count < bits; count++) {
        int sq = __builtin_ctzll(mask);
        mask &= mask - 1;
        if (index & (1 << count))
            occ |= (1ULL << sq);
    }
    return occ;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * bitboard_init
 * ──────────────────────────────────────────────────────────────────────────── */

void bitboard_init(void)
{
    /* ── knight attacks ─────────────────────────────────────────────── */
    for (int sq = 0; sq < 64; sq++) {
        uint64_t bb = 0ULL;
        int r = RANK_OF(sq), f = FILE_OF(sq);
        int dirs[8][2] = {
            {-2,-1},{-2,+1},{-1,-2},{-1,+2},
            {+1,-2},{+1,+2},{+2,-1},{+2,+1}
        };
        for (int d = 0; d < 8; d++) {
            int ff = f + dirs[d][0];
            int rr = r + dirs[d][1];
            if (ff >= 0 && ff < 8 && rr >= 0 && rr < 8)
                bb |= (1ULL << SQUARE(ff, rr));
        }
        knightAttacks[sq] = bb;
    }

    /* ── king attacks ───────────────────────────────────────────────── */
    for (int sq = 0; sq < 64; sq++) {
        uint64_t bb = 0ULL;
        int r = RANK_OF(sq), f = FILE_OF(sq);
        for (int dr = -1; dr <= 1; dr++) {
            for (int df = -1; df <= 1; df++) {
                if (dr == 0 && df == 0) continue;
                int ff = f + df;
                int rr = r + dr;
                if (ff >= 0 && ff < 8 && rr >= 0 && rr < 8)
                    bb |= (1ULL << SQUARE(ff, rr));
            }
        }
        kingAttacks[sq] = bb;
    }

    /* ── pawn attacks ───────────────────────────────────────────────── */
    for (int sq = 0; sq < 64; sq++) {
        int r = RANK_OF(sq), f = FILE_OF(sq);

        /* white attacks (north-west, north-east) */
        uint64_t w = 0ULL;
        if (r < 7 && f > 0) w |= (1ULL << SQUARE(f-1, r+1));
        if (r < 7 && f < 7) w |= (1ULL << SQUARE(f+1, r+1));
        pawnAttacks[COLOR_IDX(WHITE)][sq] = w;

        /* black attacks (south-west, south-east) */
        uint64_t b = 0ULL;
        if (r > 0 && f > 0) b |= (1ULL << SQUARE(f-1, r-1));
        if (r > 0 && f < 7) b |= (1ULL << SQUARE(f+1, r-1));
        pawnAttacks[COLOR_IDX(BLACK)][sq] = b;
    }

    /* ── rook magic ─────────────────────────────────────────────────── */
    for (int sq = 0; sq < 64; sq++) {
        uint64_t mask = rook_mask(sq);
        int bits = bit_count(mask);
        int table_size = 1 << bits;
        rook_table[sq] = (uint64_t *)malloc((size_t)table_size * sizeof(uint64_t));
        if (!rook_table[sq]) {
            fprintf(stderr, "malloc failed for rook_table[%d]\n", sq);
            exit(1);
        }
        for (int i = 0; i < table_size; i++) {
            uint64_t occ = set_occupancy(i, bits, mask);
            int index = (int)((occ * rook_magics[sq]) >> (64 - rook_shift[sq]));
            rook_table[sq][index] = rook_attacks_raw(sq, occ);
        }
    }

    /* ── bishop magic ───────────────────────────────────────────────── */
    for (int sq = 0; sq < 64; sq++) {
        uint64_t mask = bishop_mask(sq);
        int bits = bit_count(mask);
        int table_size = 1 << bits;
        bishop_table[sq] = (uint64_t *)malloc((size_t)table_size * sizeof(uint64_t));
        if (!bishop_table[sq]) {
            fprintf(stderr, "malloc failed for bishop_table[%d]\n", sq);
            exit(1);
        }
        for (int i = 0; i < table_size; i++) {
            uint64_t occ = set_occupancy(i, bits, mask);
            int index = (int)((occ * bishop_magics[sq]) >> (64 - bishop_shift[sq]));
            bishop_table[sq][index] = bishop_attacks_raw(sq, occ);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Magic attack getters
 * ──────────────────────────────────────────────────────────────────────────── */

uint64_t rookAttacks(int sq, uint64_t occ)
{
    uint64_t mask = rook_mask(sq);
    occ &= mask;
    int index = (int)((occ * rook_magics[sq]) >> (64 - rook_shift[sq]));
    return rook_table[sq][index];
}

uint64_t bishopAttacks(int sq, uint64_t occ)
{
    uint64_t mask = bishop_mask(sq);
    occ &= mask;
    int index = (int)((occ * bishop_magics[sq]) >> (64 - bishop_shift[sq]));
    return bishop_table[sq][index];
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Position functions
 * ──────────────────────────────────────────────────────────────────────────── */

void position_startpos(Position *pos)
{
    memset(pos, 0, sizeof(Position));

    /* white pieces */
    pos->pieces[COLOR_IDX(WHITE)][PAWN]   = 0x000000000000FF00ULL;  /* rank 2 */
    pos->pieces[COLOR_IDX(WHITE)][ROOK]   = 0x0000000000000081ULL;  /* A1,H1 */
    pos->pieces[COLOR_IDX(WHITE)][KNIGHT] = 0x0000000000000042ULL;  /* B1,G1 */
    pos->pieces[COLOR_IDX(WHITE)][BISHOP] = 0x0000000000000024ULL;  /* C1,F1 */
    pos->pieces[COLOR_IDX(WHITE)][QUEEN]  = 0x0000000000000008ULL;  /* D1 */
    pos->pieces[COLOR_IDX(WHITE)][KING]   = 0x0000000000000010ULL;  /* E1 */

    /* black pieces */
    pos->pieces[COLOR_IDX(BLACK)][PAWN]   = 0x00FF000000000000ULL;  /* rank 7 */
    pos->pieces[COLOR_IDX(BLACK)][ROOK]   = 0x8100000000000000ULL;  /* A8,H8 */
    pos->pieces[COLOR_IDX(BLACK)][KNIGHT] = 0x4200000000000000ULL;  /* B8,G8 */
    pos->pieces[COLOR_IDX(BLACK)][BISHOP] = 0x2400000000000000ULL;  /* C8,F8 */
    pos->pieces[COLOR_IDX(BLACK)][QUEEN]  = 0x0800000000000000ULL;  /* D8 */
    pos->pieces[COLOR_IDX(BLACK)][KING]   = 0x1000000000000000ULL;  /* E8 */

    /* occupancy */
    for (int pt = PAWN; pt <= KING; pt++) {
        pos->occ[COLOR_IDX(WHITE)] |= pos->pieces[COLOR_IDX(WHITE)][pt];
        pos->occ[COLOR_IDX(BLACK)] |= pos->pieces[COLOR_IDX(BLACK)][pt];
    }
    pos->occAll = pos->occ[COLOR_IDX(WHITE)] | pos->occ[COLOR_IDX(BLACK)];

    /* mailbox board */
    for (int sq = 0; sq < 64; sq++) {
        uint64_t bit = 1ULL << sq;
        Piece p = EMPTY;
        for (int c = 0; c < 2; c++) {
            for (int pt = PAWN; pt <= KING; pt++) {
                if (pos->pieces[c][pt] & bit) {
                    p = MAKE_PIECE((c == 0 ? WHITE : BLACK), (PieceType)pt);
                    break;
                }
            }
            if (p != EMPTY) break;
        }
        pos->board[sq] = p;
    }

    /* king squares */
    pos->kingSq[COLOR_IDX(WHITE)] = E1;
    pos->kingSq[COLOR_IDX(BLACK)] = E8;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Position print
 * ──────────────────────────────────────────────────────────────────────────── */

static const char pt_char[] = {
    '.',            /* NONE   */
    'P',            /* PAWN   */
    'N',            /* KNIGHT */
    'B',            /* BISHOP */
    'R',            /* ROOK   */
    'Q',            /* QUEEN  */
    'K'             /* KING   */
};

void position_print(const Position *pos)
{
    for (int rank = 7; rank >= 0; rank--) {
        printf("  +---+---+---+---+---+---+---+---+\n");
        printf("%d |", rank + 1);
        for (int file = 0; file < 8; file++) {
            Piece p = pos->board[rank * 8 + file];
            char ch = pt_char[PIECE_TYPE(p)];
            if (PIECE_COLOR(p) == BLACK)
                ch = (char)tolower((unsigned char)ch);
            printf(" %c |", ch);
        }
        printf("\n");
    }
    printf("  +---+---+---+---+---+---+---+---+\n");
    printf("    a   b   c   d   e   f   g   h\n\n");
}