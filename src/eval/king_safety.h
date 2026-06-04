#ifndef KING_SAFETY_H
#define KING_SAFETY_H

#include "boards.h"

void init_king_safety_tables(void);
int evaluate_king_safety(const Position *pos, Color color, int phase);

#endif /* KING_SAFETY_H */
