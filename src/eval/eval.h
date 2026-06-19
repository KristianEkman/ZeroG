#ifndef EVAL_H
#define EVAL_H

#include "boards.h"
#include "nn.h"
#include <stdbool.h>

extern bool use_nn;
extern NeuralNetwork *eval_nn;

void eval_init(void);
void eval_free(void);

/*
 * Evaluates the position from White's perspective.
 * Positive values are favorable for White, negative values for Black.
 * Returns the evaluation score in centipawns.
 */
int evaluate(const Position *pos);

#endif /* EVAL_H */
