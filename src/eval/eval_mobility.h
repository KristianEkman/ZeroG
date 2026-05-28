#ifndef EVAL_MOBILITY_H
#define EVAL_MOBILITY_H

#include "boards.h"

/*
 * Evaluates piece mobility for both colors and returns the difference
 * (White mobility score - Black mobility score) in centipawns.
 */
int evaluate_mobility(const Position *pos);

#endif /* EVAL_MOBILITY_H */
