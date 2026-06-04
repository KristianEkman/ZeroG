#ifndef PAWN_EVAL_H
#define PAWN_EVAL_H

#include "boards.h"

/*
 * Evaluates pawn structures (PST, passed pawns, isolated pawns, doubled pawns)
 * for both colors and returns the relative score (White - Black) in centipawns.
 */
int evaluate_pawns(const Position *pos, int phase);

#endif /* PAWN_EVAL_H */
