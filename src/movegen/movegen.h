#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "boards.h"

/* Maximum pseudo-legal moves in any chess position (theoretical max ~218) */
#define MAX_MOVES 256

/* ── undo state ──────────────────────────────────────────────────────────── */

typedef struct {
    Move     move;
    Piece    moving;        /* piece on the source square before the move     */
    Piece    captured;      /* captured piece, or EMPTY                       */
    int      cap_sq;        /* square where the captured piece resided        */
    int      old_ep;        /* en passant square before the move              */
    uint8_t  old_castling;  /* castling rights before the move                */
    int      old_fifty;     /* halfmove clock before the move                 */
} Undo;

/* ── make / unmake a move on a position ──────────────────────────────────── */

/* Apply a move to the position.  Writes the necessary undo information
 * into `u` so that `undo_move(pos, &u)` can later restore the position. */
void apply_move(Position *pos, Move m, Undo *u);

/* Restore the position to the state it had immediately before the move
 * described by `u` was applied. */
void undo_move(Position *pos, const Undo *u);

/* ── move generation ─────────────────────────────────────────────────────── */

/* Generate all pseudo-legal moves for the side to move.
 * Returns the number of moves written to `moves[]`.
 * `moves` must have room for at least MAX_MOVES entries. */
int movegen_pseudo_legal(const Position *pos, Move *moves);

/* Generate only legal moves (pseudo-legal moves that do not leave own king in check).
 * Returns the number of moves written to `moves[]`.
 * `moves` must have room for at least MAX_MOVES entries. */
int movegen_legal(const Position *pos, Move *moves);

/* Test whether `sq` is attacked by `attacker` in `pos`.
 * Useful externally (e.g., for castling legality, check detection). */
int is_square_attacked(const Position *pos, int sq, Color attacker);

/* ── perft (performance test) ───────────────────────────────────────────── */

/* Count the number of legal leaf nodes at the given depth from `pos`.
 * Makes and unmakes every move to traverse the tree exhaustively.
 * Used to validate move generation correctness against known perft numbers. */
uint64_t perft(Position *pos, int depth);

#endif /* MOVEGEN_H */