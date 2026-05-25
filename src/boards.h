#ifndef BOARDS_H
#define BOARDS_H

#include <stdint.h>

/* ── squares (A1=0 … H8=63) ─────────────────────────────────────────────── */
typedef enum {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQUARE_NB = 64
} Square;

/* ── files and ranks ────────────────────────────────────────────────────── */
enum {
    FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H,
    FILE_NB = 8
};
enum {
    RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8,
    RANK_NB = 8
};

#define FILE_OF(sq)  ((sq) & 7)
#define RANK_OF(sq)  ((sq) >> 3)
#define SQUARE(f,r)  ((r) * 8 + (f))

/* ── colour ─────────────────────────────────────────────────────────────── */
typedef enum {
    WHITE = 0,
    BLACK = 8
} Color;

#define COLOR_IDX(c)  ((c) >> 3)       /* WHITE=0 → 0, BLACK=8 → 1 */
#define OPPOSITE(c)   ((Color)((c) ^ 8))

/* ── piece type ─────────────────────────────────────────────────────────── */
typedef enum {
    NONE   = 0,
    PAWN   = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK   = 4,
    QUEEN  = 5,
    KING   = 6,
    PIECE_TYPE_NB = 7
} PieceType;

/* ── castling rights ──────────────────────────────────────────────────── */
#define CASTLE_WK  1
#define CASTLE_WQ  2
#define CASTLE_BK  4
#define CASTLE_BQ  8
#define CASTLE_ALL 15

/* ── en passant sentinel ──────────────────────────────────────────────── */
#define SQ_NONE  (-1)

/* ── piece encoding (4 bits: colour bit3 + type bits0-2) ───────────────── */
typedef uint8_t Piece;

#define PIECE_TYPE(p)    ((p) & 7)
#define PIECE_COLOR(p)   ((p) & 8)
#define MAKE_PIECE(c, t) ((Piece)((c) | (t)))

/* pre-built piece constants */
enum {
    EMPTY     = 0,
    W_PAWN   = 0 | PAWN,
    W_KNIGHT = 0 | KNIGHT,
    W_BISHOP = 0 | BISHOP,
    W_ROOK   = 0 | ROOK,
    W_QUEEN  = 0 | QUEEN,
    W_KING   = 0 | KING,
    B_PAWN   = 8 | PAWN,
    B_KNIGHT = 8 | KNIGHT,
    B_BISHOP = 8 | BISHOP,
    B_ROOK   = 8 | ROOK,
    B_QUEEN  = 8 | QUEEN,
    B_KING   = 8 | KING
};

/* ── move (16 bits) ────────────────────────────────────────────────────────
 * bits 0-5   : from square
 * bits 6-11  : to square
 * bits 12-13 : promotion piece type (0=N, 1=B, 2=R, 3=Q) – only for promos
 * bits 14-15 : flag
 *   0 = quiet move
 *   1 = double pawn push
 *   2 = king-side castling
 *   3 = queen-side castling
 *   (bit 12=1 indicates promotion; bit 13=1 indicates capture)
 * ─────────────────────────────────────────────────────────────────────── */
typedef uint16_t Move;

#define MOVE_FROM(m)      ((m) & 0x3F)
#define MOVE_TO(m)        (((m) >> 6) & 0x3F)
#define MOVE_PROMO(m)     (((m) >> 12) & 3)
#define MOVE_FLAG(m)      ((m) >> 14)
#define MOVE_BUILD(f,t,p,fl) ((Move)((f) | ((t) << 6) | ((p) << 12) | ((fl) << 14)))

enum {
    MOVE_QUIET       = 0,
    MOVE_DOUBLE_PUSH = 1,
    MOVE_CASTLE_KS   = 2,
    MOVE_CASTLE_QS   = 3
};

/* ── board (hybrid bitboard + mailbox) ──────────────────────────────────── */
typedef struct {
    uint64_t pieces[2][7];   /* [COLOR_IDX(color)][PAWN..KING]   */
    uint64_t occ[2];         /* [WHITE_IDX], [BLACK_IDX]         */
    uint64_t occAll;         /* all occupied squares             */
    Piece    board[64];      /* mailbox helper                   */
    int      kingSq[2];      /* king square for each colour      */
    Color    sideToMove;     /* side to move next                */
    uint8_t  castlingRights; /* castling availability bitmask    */
    int      enPassantSquare;/* en passant target square or SQ_NONE */
    int      fiftyMoveCounter;/* halfmove clock                   */
    int      fullmoveNumber; /* fullmove number                   */
    uint64_t hashKey;        /* zobrist hash of the position      */
} Position;

/* ── attack tables (initialised by bitboard_init) ──────────────────────── */
extern uint64_t knightAttacks[64];
extern uint64_t kingAttacks[64];
extern uint64_t pawnAttacks[2][64];

/* ── magic bitboard attack getters ─────────────────────────────────────── */
uint64_t rookAttacks(int sq, uint64_t occ);
uint64_t bishopAttacks(int sq, uint64_t occ);
static inline uint64_t queenAttacks(int sq, uint64_t occ) {
    return rookAttacks(sq, occ) | bishopAttacks(sq, occ);
}

/* ── bit utility ───────────────────────────────────────────────────────── */
static inline int pop_lsb(uint64_t *bb) {
    int sq = __builtin_ctzll(*bb);
    *bb &= *bb - 1;
    return sq;
}
static inline int bit_count(uint64_t bb) {
    return __builtin_popcountll(bb);
}

/* ── one-time initialisation (must be called before any Position use) ─── */
void bitboard_init(void);

/* ── position ──────────────────────────────────────────────────────────── */
void position_startpos(Position *pos);
void position_print(const Position *pos);

#endif /* BOARDS_H */