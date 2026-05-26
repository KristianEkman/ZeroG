#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "unity.h"
#include "movegen.h"
#include "fen.h"
#include "zobrist.h"
#include <string.h>

extern Position pos;

int contains_move(const Move *moves, int n, Move m);
void assert_positions_equal(const Position *saved, const Position *pos);

#endif /* TEST_HELPERS_H */
