#ifndef KING_SAFETY_H
#define KING_SAFETY_H

#include "boards.h"

int evaluate_king_safety(const Position *pos, Color color, int is_endgame);

#endif /* KING_SAFETY_H */
