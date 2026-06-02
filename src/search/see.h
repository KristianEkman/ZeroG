#ifndef SEE_H
#define SEE_H

#include "boards.h"

/* Returns the static exchange evaluation score of a move on the target square.
   A positive value indicates a net material gain, zero is equal, and negative is a loss. */
int see(const Position *pos, Move m);

#endif /* SEE_H */
