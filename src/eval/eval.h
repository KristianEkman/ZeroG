#ifndef EVAL_H
#define EVAL_H

#include "boards.h"

/*
 * Evaluates the position from White's perspective.
 * Positive values are favorable for White, negative values for Black.
 * Returns the evaluation score in centipawns.
 */
int evaluate(const Position *pos);

#endif /* EVAL_H */
