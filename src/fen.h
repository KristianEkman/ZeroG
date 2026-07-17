#ifndef FEN_H
#define FEN_H

#include "boards.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * FEN parsing / serialisation
 * ──────────────────────────────────────────────────────────────────────────── */

/* Parse a FEN string into a Position.
 * Returns 0 on success, -1 on parse error.
 * The caller is responsible for zero-initialising *pos before the call
 * (or calling this on a fresh Position obtained from position_startpos). */
int fen_parse(const char *fen, Position *pos);

/* Serialise a Position into a FEN string.
 * `buf` must be at least 92 bytes (worst case: 71 + 2 + 1 + 2 + 3 + 3 + 1).
 * Returns the number of characters written (excluding NUL),
 * or -1 if buf is NULL. */
int fen_serialize(const Position *pos, char *buf);

/* Parse an EPD line, populating position and target score (normalized to pawn units)
 * Returns 0 on success, or:
 * -1: No score opcode
 * -2: Mate score
 * -3: Extreme score
 * -4: FEN parse error
 * -5: Malformed score label */
int parse_epd_line(const char *line, Position *pos, float *target);

#endif /* FEN_H */