#include "boards.h"
#include <ctype.h>
#include <stdio.h>

/* piece-type → display character (uppercase for white) */
static const char pt_char[] = {
    '.',            /* NONE   */
    'P',            /* PAWN   */
    'N',            /* KNIGHT */
    'B',            /* BISHOP */
    'R',            /* ROOK   */
    'Q',            /* QUEEN  */
    'K'             /* KING   */
};

/* ── board_startpos ───────────────────────────────────────────────────── */
void board_startpos(Board *board)
{
    /* clear all squares */
    for (int sq = 0; sq < SQUARE_NB; sq++)
        board->squares[sq] = EMPTY;

    /* ranks 1 and 8 */
    board->squares[A1] = W_ROOK;   board->squares[H1] = W_ROOK;
    board->squares[B1] = W_KNIGHT; board->squares[G1] = W_KNIGHT;
    board->squares[C1] = W_BISHOP; board->squares[F1] = W_BISHOP;
    board->squares[D1] = W_QUEEN;  board->squares[E1] = W_KING;

    board->squares[A8] = B_ROOK;   board->squares[H8] = B_ROOK;
    board->squares[B8] = B_KNIGHT; board->squares[G8] = B_KNIGHT;
    board->squares[C8] = B_BISHOP; board->squares[F8] = B_BISHOP;
    board->squares[D8] = B_QUEEN;  board->squares[E8] = B_KING;

    /* pawns – rank 2 (white) */
    for (int sq = A2; sq <= H2; sq++)
        board->squares[sq] = W_PAWN;

    /* pawns – rank 7 (black) */
    for (int sq = A7; sq <= H7; sq++)
        board->squares[sq] = B_PAWN;
}

/* ── board_print ──────────────────────────────────────────────────────── */
void board_print(const Board *board)
{
    /* print ranks 8 down to 1 so white is at the bottom */
    for (int rank = 7; rank >= 0; rank--) {
        printf("  +---+---+---+---+---+---+---+---+\n");
        printf("%d |", rank + 1);
        for (int file = 0; file < 8; file++) {
            uint8_t p = board->squares[rank * 8 + file];
            char ch = pt_char[PIECE_TYPE(p)];
            /* black pieces use lowercase */
            if (PIECE_COLOR(p) == BLACK)
                ch = (char)tolower((unsigned char)ch);
            printf(" %c |", ch);
        }
        printf("\n");
    }
    printf("  +---+---+---+---+---+---+---+---+\n");
    printf("    a   b   c   d   e   f   g   h\n\n");
}