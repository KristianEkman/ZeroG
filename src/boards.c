#include "boards.h"
#include "zobrist.h"
#include "eval.h"
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
const int rook_shift[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12};

/* Rook magics (found with trial and error / pre-generated) */
const uint64_t rook_magics[64] = {
    0x0080002080104000ULL,     0x0040004010002000ULL,     0x0880081001200380ULL,     0x0900209000090004ULL,
    0x1200020020040811ULL,     0x4100080244004100ULL,     0x0C00020830208401ULL,     0x05000282204A0500ULL,
    0x0020802080004008ULL,     0x045240100043E000ULL,     0x4201802000809000ULL,     0x0001000A21041000ULL,
    0x4022000804220010ULL,     0x0102001004020008ULL,     0x0C21000200048100ULL,     0x0320800041000080ULL,
    0x08C0808000400024ULL,     0x03A0004040003000ULL,     0x0000820012204600ULL,     0x0430004040080400ULL,
    0x200800C00C020040ULL,     0x0084008080040200ULL,     0x0050A40022013008ULL,     0x0484020010408924ULL,
    0x1040004080002080ULL,     0x00200140C0003000ULL,     0x0000200080801000ULL,     0x4010041080080080ULL,
    0x4005004500300800ULL,     0x1202000200081004ULL,     0x0408511400028810ULL,     0x1088008200040041ULL,
    0x0C00804010800020ULL,     0x2940002800201000ULL,     0x9000104101002004ULL,     0x8942821004800800ULL,
    0x0024008004800802ULL,     0x0210040080800200ULL,     0x8141021014004108ULL,     0x201004408E000421ULL,
    0x2380002000404003ULL,     0x1010005020004000ULL,     0x5800200010008080ULL,     0x80000A0020420012ULL,
    0x4005000800050010ULL,     0x4812010850220004ULL,     0x0010889002040001ULL,     0x000110890446002CULL,
    0x480C310080420200ULL,     0x0000201000400040ULL,     0x8010002000801080ULL,     0x0C40100025000900ULL,
    0x0481009008000500ULL,     0x8880800200040080ULL,     0x4010290850120400ULL,     0x8001000200408100ULL,
    0x5001008810402202ULL,     0x8242041080410822ULL,     0x0084304841022001ULL,     0x0A08090004201001ULL,
    0x9013008208001005ULL,     0x0002000410010802ULL,     0x0004081000C10204ULL,     0x0608040500244092ULL,
};

/* Rook attack tables: one variable-sized block per square */
uint64_t *rook_magic_table[64];
uint64_t rook_masks[64];

/* ─────────────────────────────────────────────────────────────────────────────
 * Magic bitboards – bishop
 * ──────────────────────────────────────────────────────────────────────────── */

const int bishop_shift[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6};

const uint64_t bishop_magics[64] = {
    0x4020208100408080ULL,     0x4008120086020504ULL,     0xB821020C00480000ULL,     0x8111040085200000ULL,
    0x0204042080000304ULL,     0x0002300420015258ULL,     0x0202023002C82020ULL,     0x1204804110312008ULL,
    0x4004042048820080ULL,     0x0000044810940080ULL,     0x4009C10401004000ULL,     0x140004050A001000ULL,
    0x0000C110400091C4ULL,     0x0000020202230200ULL,     0x0002020130031003ULL,     0xAA00108208010404ULL,
    0x0184444045880200ULL,     0x00080260080080A0ULL,     0x001000090C082440ULL,     0x0001040820420000ULL,
    0x0024002A01210148ULL,     0x0CC60005010101C1ULL,     0x0042800432880800ULL,     0x400200070080B400ULL,
    0x4003080190200801ULL,     0x4498228020028E00ULL,     0x0804100001010024ULL,     0x1002020020080808ULL,
    0x4020404014010040ULL,     0x0008020080405210ULL,     0x1190810804110832ULL,     0x040330A002020306ULL,
    0x0182022182400900ULL,     0x9000905001050400ULL,     0x5082080400084240ULL,     0x80000808000A0A00ULL,
    0x0001100400008020ULL,     0x0120140020210080ULL,     0x8010010A02004234ULL,     0x0001204280010400ULL,
    0x460104500580C000ULL,     0x100C088210008908ULL,     0x0006141088031000ULL,     0x0210002019008800ULL,
    0x0018100922021010ULL,     0x8490201800400488ULL,     0x121124080440008CULL,     0x0090010101100020ULL,
    0x4212542A20101002ULL,     0x0202111401040000ULL,     0x6052244404040042ULL,     0x8000000484042000ULL,
    0x14880010C2020224ULL,     0x0400082008408804ULL,     0x10480228082121A0ULL,     0x0220010440908000ULL,
    0x4000110910101400ULL,     0x0019050101012000ULL,     0x00000A0080480801ULL,     0x504A008004840404ULL,
    0x0010040091020200ULL,     0x200C41501010A120ULL,     0x0400A14204010409ULL,     0x6108101000410020ULL,
};

uint64_t *bishop_magic_table[64];
uint64_t bishop_masks[64];



/* ─────────────────────────────────────────────────────────────────────────────
 * Sliding attack masks (without magic)
 * ──────────────────────────────────────────────────────────────────────────── */

static uint64_t rook_mask(int sq)
{
    uint64_t mask = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);
    /* north */
    for (int rr = r + 1; rr <= 6; rr++)
        mask |= (1ULL << SQUARE(f, rr));
    /* south */
    for (int rr = r - 1; rr >= 1; rr--)
        mask |= (1ULL << SQUARE(f, rr));
    /* east */
    for (int ff = f + 1; ff <= 6; ff++)
        mask |= (1ULL << SQUARE(ff, r));
    /* west */
    for (int ff = f - 1; ff >= 1; ff--)
        mask |= (1ULL << SQUARE(ff, r));
    return mask;
}

static uint64_t bishop_mask(int sq)
{
    uint64_t mask = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);
    /* north-east */
    for (int rr = r + 1, ff = f + 1; rr <= 6 && ff <= 6; rr++, ff++)
        mask |= (1ULL << SQUARE(ff, rr));
    /* north-west */
    for (int rr = r + 1, ff = f - 1; rr <= 6 && ff >= 1; rr++, ff--)
        mask |= (1ULL << SQUARE(ff, rr));
    /* south-east */
    for (int rr = r - 1, ff = f + 1; rr >= 1 && ff <= 6; rr--, ff++)
        mask |= (1ULL << SQUARE(ff, rr));
    /* south-west */
    for (int rr = r - 1, ff = f - 1; rr >= 1 && ff >= 1; rr--, ff--)
        mask |= (1ULL << SQUARE(ff, rr));
    return mask;
}

/* Generate sliding attacks naively (given full occupancy) */
static uint64_t rook_attacks_raw(int sq, uint64_t occ)
{
    uint64_t att = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);
    /* north */
    for (int rr = r + 1; rr <= 7; rr++)
    {
        uint64_t bb = 1ULL << SQUARE(f, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    /* south */
    for (int rr = r - 1; rr >= 0; rr--)
    {
        uint64_t bb = 1ULL << SQUARE(f, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    /* east */
    for (int ff = f + 1; ff <= 7; ff++)
    {
        uint64_t bb = 1ULL << SQUARE(ff, r);
        att |= bb;
        if (occ & bb)
            break;
    }
    /* west */
    for (int ff = f - 1; ff >= 0; ff--)
    {
        uint64_t bb = 1ULL << SQUARE(ff, r);
        att |= bb;
        if (occ & bb)
            break;
    }
    return att;
}

static uint64_t bishop_attacks_raw(int sq, uint64_t occ)
{
    uint64_t att = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);
    /* north-east */
    for (int rr = r + 1, ff = f + 1; rr <= 7 && ff <= 7; rr++, ff++)
    {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    /* north-west */
    for (int rr = r + 1, ff = f - 1; rr <= 7 && ff >= 0; rr++, ff--)
    {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    /* south-east */
    for (int rr = r - 1, ff = f + 1; rr >= 0 && ff <= 7; rr--, ff++)
    {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    /* south-west */
    for (int rr = r - 1, ff = f - 1; rr >= 0 && ff >= 0; rr--, ff--)
    {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    return att;
}

/* Generate all occupancy permutations from a mask */
static uint64_t set_occupancy(int index, int bits, uint64_t mask)
{
    uint64_t occ = 0ULL;
    for (int count = 0; count < bits; count++)
    {
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
    for (int sq = 0; sq < 64; sq++)
    {
        uint64_t bb = 0ULL;
        int r = RANK_OF(sq), f = FILE_OF(sq);
        int dirs[8][2] = {
            {-2, -1}, {-2, +1}, {-1, -2}, {-1, +2}, {+1, -2}, {+1, +2}, {+2, -1}, {+2, +1}};
        for (int d = 0; d < 8; d++)
        {
            int ff = f + dirs[d][0];
            int rr = r + dirs[d][1];
            if (ff >= 0 && ff < 8 && rr >= 0 && rr < 8)
                bb |= (1ULL << SQUARE(ff, rr));
        }
        knightAttacks[sq] = bb;
    }

    /* ── king attacks ───────────────────────────────────────────────── */
    for (int sq = 0; sq < 64; sq++)
    {
        uint64_t bb = 0ULL;
        int r = RANK_OF(sq), f = FILE_OF(sq);
        for (int dr = -1; dr <= 1; dr++)
        {
            for (int df = -1; df <= 1; df++)
            {
                if (dr == 0 && df == 0)
                    continue;
                int ff = f + df;
                int rr = r + dr;
                if (ff >= 0 && ff < 8 && rr >= 0 && rr < 8)
                    bb |= (1ULL << SQUARE(ff, rr));
            }
        }
        kingAttacks[sq] = bb;
    }

    /* ── pawn attacks ───────────────────────────────────────────────── */
    for (int sq = 0; sq < 64; sq++)
    {
        int r = RANK_OF(sq), f = FILE_OF(sq);

        /* white attacks (north-west, north-east) */
        uint64_t w = 0ULL;
        if (r < 7 && f > 0)
            w |= (1ULL << SQUARE(f - 1, r + 1));
        if (r < 7 && f < 7)
            w |= (1ULL << SQUARE(f + 1, r + 1));
        pawnAttacks[COLOR_IDX(WHITE)][sq] = w;

        /* black attacks (south-west, south-east) */
        uint64_t b = 0ULL;
        if (r > 0 && f > 0)
            b |= (1ULL << SQUARE(f - 1, r - 1));
        if (r > 0 && f < 7)
            b |= (1ULL << SQUARE(f + 1, r - 1));
        pawnAttacks[COLOR_IDX(BLACK)][sq] = b;
    }

    /* ── rook magic ─────────────────────────────────────────────────── */
    for (int sq = 0; sq < 64; sq++)
    {
        uint64_t mask = rook_mask(sq);
        int bits = bit_count(mask);
        int table_size = 1 << bits;
        int permutations = 1 << bits;

        rook_masks[sq] = mask;
        rook_magic_table[sq] = (uint64_t *)calloc((size_t)table_size, sizeof(uint64_t));
        if (!rook_magic_table[sq])
        {
            fprintf(stderr, "malloc failed for rook_magic_table[%d]\n", sq);
            exit(1);
        }

        for (int i = 0; i < permutations; i++)
        {
            uint64_t occ = set_occupancy(i, bits, mask);
            uint64_t attacks = rook_attacks_raw(sq, occ);
            int index = (int)((occ * rook_magics[sq]) >> (64 - rook_shift[sq]));

            if (index < 0 || index >= table_size)
            {
                fprintf(stderr,
                        "rook index out of range: sq=%d i=%d index=%d size=%d\n",
                        sq, i, index, table_size);
                exit(1);
            }

            rook_magic_table[sq][index] = attacks;
        }
    }

    /* ── bishop magic ───────────────────────────────────────────────── */
    for (int sq = 0; sq < 64; sq++)
    {
        uint64_t mask = bishop_mask(sq);
        int bits = bit_count(mask);
        int table_size = 1 << bits;
        int permutations = 1 << bits;

        bishop_masks[sq] = mask;
        bishop_magic_table[sq] = (uint64_t *)calloc((size_t)table_size, sizeof(uint64_t));
        if (!bishop_magic_table[sq])
        {
            fprintf(stderr, "malloc failed for bishop_magic_table[%d]\n", sq);
            exit(1);
        }

        for (int i = 0; i < permutations; i++)
        {
            uint64_t occ = set_occupancy(i, bits, mask);
            uint64_t attacks = bishop_attacks_raw(sq, occ);
            int index = (int)((occ * bishop_magics[sq]) >> (64 - bishop_shift[sq]));

            if (index < 0 || index >= table_size)
            {
                fprintf(stderr,
                        "bishop index out of range: sq=%d i=%d index=%d size=%d\n",
                        sq, i, index, table_size);
                exit(1);
            }

            bishop_magic_table[sq][index] = attacks;
        }
    }

    /* Initialize Zobrist hashing keys */
    zobrist_init();

    /* Initialize evaluation (load NN weights if available) */
    eval_init();
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Magic attack getters (inlined in boards.h)
 * ──────────────────────────────────────────────────────────────────────────── */

/* ─────────────────────────────────────────────────────────────────────────────
 * Position functions
 * ──────────────────────────────────────────────────────────────────────────── */

void position_startpos(Position *pos)
{
    memset(pos, 0, sizeof(Position));

    /* white pieces */
    pos->pieces[COLOR_IDX(WHITE)][PAWN] = 0x000000000000FF00ULL;   /* rank 2 */
    pos->pieces[COLOR_IDX(WHITE)][ROOK] = 0x0000000000000081ULL;   /* A1,H1 */
    pos->pieces[COLOR_IDX(WHITE)][KNIGHT] = 0x0000000000000042ULL; /* B1,G1 */
    pos->pieces[COLOR_IDX(WHITE)][BISHOP] = 0x0000000000000024ULL; /* C1,F1 */
    pos->pieces[COLOR_IDX(WHITE)][QUEEN] = 0x0000000000000008ULL;  /* D1 */
    pos->pieces[COLOR_IDX(WHITE)][KING] = 0x0000000000000010ULL;   /* E1 */

    /* black pieces */
    pos->pieces[COLOR_IDX(BLACK)][PAWN] = 0x00FF000000000000ULL;   /* rank 7 */
    pos->pieces[COLOR_IDX(BLACK)][ROOK] = 0x8100000000000000ULL;   /* A8,H8 */
    pos->pieces[COLOR_IDX(BLACK)][KNIGHT] = 0x4200000000000000ULL; /* B8,G8 */
    pos->pieces[COLOR_IDX(BLACK)][BISHOP] = 0x2400000000000000ULL; /* C8,F8 */
    pos->pieces[COLOR_IDX(BLACK)][QUEEN] = 0x0800000000000000ULL;  /* D8 */
    pos->pieces[COLOR_IDX(BLACK)][KING] = 0x1000000000000000ULL;   /* E8 */

    /* occupancy */
    for (int pt = PAWN; pt <= KING; pt++)
    {
        pos->occ[COLOR_IDX(WHITE)] |= pos->pieces[COLOR_IDX(WHITE)][pt];
        pos->occ[COLOR_IDX(BLACK)] |= pos->pieces[COLOR_IDX(BLACK)][pt];
    }
    pos->occAll = pos->occ[COLOR_IDX(WHITE)] | pos->occ[COLOR_IDX(BLACK)];

    /* mailbox board */
    for (int sq = 0; sq < 64; sq++)
    {
        uint64_t bit = 1ULL << sq;
        Piece p = EMPTY;
        for (int c = 0; c < 2; c++)
        {
            for (int pt = PAWN; pt <= KING; pt++)
            {
                if (pos->pieces[c][pt] & bit)
                {
                    p = MAKE_PIECE((c == 0 ? WHITE : BLACK), (PieceType)pt);
                    break;
                }
            }
            if (p != EMPTY)
                break;
        }
        pos->board[sq] = p;
    }

    /* king squares */
    pos->kingSq[COLOR_IDX(WHITE)] = E1;
    pos->kingSq[COLOR_IDX(BLACK)] = E8;

    /* game state */
    pos->sideToMove = WHITE;
    pos->castlingRights = CASTLE_ALL;
    pos->enPassantSquare = SQ_NONE;
    pos->fiftyMoveCounter = 0;
    pos->fullmoveNumber = 1;

    pos->hashKey = zobrist_compute_key(pos);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Position print
 * ──────────────────────────────────────────────────────────────────────────── */

static const char pt_char[] = {
    '.', /* NONE   */
    'P', /* PAWN   */
    'N', /* KNIGHT */
    'B', /* BISHOP */
    'R', /* ROOK   */
    'Q', /* QUEEN  */
    'K'  /* KING   */
};

void position_print(const Position *pos)
{
    for (int rank = 7; rank >= 0; rank--)
    {
        printf("  +---+---+---+---+---+---+---+---+\n");
        printf("%d |", rank + 1);
        for (int file = 0; file < 8; file++)
        {
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

    /* game state */
    printf("Side to move: %s\n", pos->sideToMove == WHITE ? "white" : "black");

    printf("Castling: ");
    if (pos->castlingRights & CASTLE_WK)
        printf("K");
    if (pos->castlingRights & CASTLE_WQ)
        printf("Q");
    if (pos->castlingRights & CASTLE_BK)
        printf("k");
    if (pos->castlingRights & CASTLE_BQ)
        printf("q");
    if (!pos->castlingRights)
        printf("-");
    printf("\n");

    printf("En passant: ");
    if (pos->enPassantSquare == SQ_NONE)
        printf("-\n");
    else
        printf("%c%c\n", 'a' + FILE_OF(pos->enPassantSquare), '1' + RANK_OF(pos->enPassantSquare));

    printf("Halfmove clock: %d\n", pos->fiftyMoveCounter);
    printf("Fullmove number: %d\n", pos->fullmoveNumber);
}