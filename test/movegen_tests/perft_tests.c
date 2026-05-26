#include "test_helpers.h"

/* ── start position ─────────────────────────────────────────────────── */
void test_perft_startpos_depth0(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(1ULL, perft(&pos, 0));
}

void test_perft_startpos_depth1(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(20ULL, perft(&pos, 1));
}

void test_perft_startpos_depth2(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(400ULL, perft(&pos, 2));
}

void test_perft_startpos_depth3(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(8902ULL, perft(&pos, 3));
}

void test_perft_startpos_depth4(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(197281ULL, perft(&pos, 4));
}

/* ── Kiwipete ────────────────────────────────────────────────────────── */
void test_perft_kiwipete_depth1(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", &p));
    TEST_ASSERT_EQUAL_UINT64(48ULL, perft(&p, 1));
}

void test_perft_kiwipete_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", &p));
    TEST_ASSERT_EQUAL_UINT64(2039ULL, perft(&p, 2));
}

void test_perft_kiwipete_depth3(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", &p));
    TEST_ASSERT_EQUAL_UINT64(97862ULL, perft(&p, 3));
}

/* ── Position 3 ──────────────────────────────────────────────────────── */
void test_perft_pos3_depth1(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", &p));
    TEST_ASSERT_EQUAL_UINT64(14ULL, perft(&p, 1));
}

void test_perft_pos3_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", &p));
    TEST_ASSERT_EQUAL_UINT64(191ULL, perft(&p, 2));
}

void test_perft_pos3_depth3(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", &p));
    TEST_ASSERT_EQUAL_UINT64(2812ULL, perft(&p, 3));
}

/* ── Position 4 ──────────────────────────────────────────────────────── */
void test_perft_pos4_depth1(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", &p));
    TEST_ASSERT_EQUAL_UINT64(6ULL, perft(&p, 1));
}

void test_perft_pos4_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", &p));
    TEST_ASSERT_EQUAL_UINT64(264ULL, perft(&p, 2));
}

void test_perft_pos4_depth3(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", &p));
    TEST_ASSERT_EQUAL_UINT64(9467ULL, perft(&p, 3));
}

/* ── Position 5 ──────────────────────────────────────────────────────── */
void test_perft_pos5_depth1(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", &p));
    TEST_ASSERT_EQUAL_UINT64(44ULL, perft(&p, 1));
}

void test_perft_pos5_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", &p));
    TEST_ASSERT_EQUAL_UINT64(1486ULL, perft(&p, 2));
}

/* ── Position 6 ──────────────────────────────────────────────────────── */
void test_perft_pos6_depth1(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", &p));
    TEST_ASSERT_EQUAL_UINT64(46ULL, perft(&p, 1));
}

void test_perft_pos6_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", &p));
    TEST_ASSERT_EQUAL_UINT64(2079ULL, perft(&p, 2));
}
