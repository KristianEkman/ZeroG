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

/* ── colour (bit 3 in the piece encoding) ───────────────────────────────── */
typedef enum {
    WHITE = 0,
    BLACK = 8
} Color;

/* ── piece type (bits 0‑2) ──────────────────────────────────────────────── */
typedef enum {
    NONE   = 0,
    PAWN   = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK   = 4,
    QUEEN  = 5,
    KING   = 6
} PieceType;

/* ── convenience macros ───────────────────────────────────────────────── */
#define PIECE_TYPE(p)    ((p) & 7)          /* bits 0‑2 → PieceType   */
#define PIECE_COLOR(p)   ((p) & 8)          /* bit  3   → Color       */
#define MAKE_PIECE(c, t) ((uint8_t)((c) | (t))) /* combine Color + PieceType */

/* pre‑built piece constants (READ ONLY) */
enum {
    EMPTY   = 0,
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

/* ── board ───────────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t squares[SQUARE_NB];
} Board;

/* initialise the board to the standard starting position */
void board_startpos(Board *board);

/* print the board to stdout (white at bottom, black at top) */
void board_print(const Board *board);

#endif /* BOARDS_H */