#include "test_helpers.h"

Position pos;

void setUp(void)
{
    position_startpos(&pos);
}

void tearDown(void) { }

int contains_move(const Move *moves, int n, Move m) {
    for (int i = 0; i < n; i++)
        if (moves[i] == m) return 1;
    return 0;
}

void assert_positions_equal(const Position *saved, const Position *pos)
{
    TEST_ASSERT_EQUAL_INT(saved->sideToMove, pos->sideToMove);
    TEST_ASSERT_EQUAL_INT(saved->castlingRights, pos->castlingRights);
    TEST_ASSERT_EQUAL_INT(saved->enPassantSquare, pos->enPassantSquare);
    TEST_ASSERT_EQUAL_INT(saved->fiftyMoveCounter, pos->fiftyMoveCounter);
    TEST_ASSERT_EQUAL_INT(saved->fullmoveNumber, pos->fullmoveNumber);
    TEST_ASSERT_EQUAL_INT(saved->kingSq[0], pos->kingSq[0]);
    TEST_ASSERT_EQUAL_INT(saved->kingSq[1], pos->kingSq[1]);
    TEST_ASSERT_EQUAL_UINT64(saved->occAll, pos->occAll);
    TEST_ASSERT_EQUAL_UINT64(saved->occ[0], pos->occ[0]);
    TEST_ASSERT_EQUAL_UINT64(saved->occ[1], pos->occ[1]);
    TEST_ASSERT_EQUAL_UINT64(saved->hashKey, pos->hashKey);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(pos), pos->hashKey);
    for (int col = 0; col < 2; col++) {
        for (int pt = 0; pt < 7; pt++) {
            TEST_ASSERT_EQUAL_UINT64(saved->pieces[col][pt], pos->pieces[col][pt]);
        }
    }
    for (int sq = 0; sq < 64; sq++) {
        TEST_ASSERT_EQUAL_UINT8(saved->board[sq], pos->board[sq]);
    }
}
