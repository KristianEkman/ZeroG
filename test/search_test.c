#include "unity.h"
#include "boards.h"
#include "fen.h"
#include "movegen.h"
#include "search/search.h"
#include "uci/uci.h"
#include <string.h>

#define MATE_SCORE 29000

void setUp(void) {}
void tearDown(void) {}

void test_search_mate_in_1(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    // FEN where White is one move away from mate: Qh6-h8#
    // Rook on G7 prevents King from escaping to 7th rank.
    int parse_res = fen_parse("k7/6R1/7Q/8/8/8/8/6K1 w - - 0 1", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    SearchLimits limits = {
        .depth = 3,
        .soft_time_limit_ms = 0,
        .hard_time_limit_ms = 0
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);
    TEST_ASSERT_TRUE(result.has_legal_move);

    // Convert best move to string
    char move_str[6];
    int str_res = uci_move_to_string(&test_pos, result.best_move, move_str, sizeof(move_str));
    TEST_ASSERT_EQUAL_INT(0, str_res);
    // Qh6 is at H6 (index 47). Qh8 is at H8 (index 63). So move string should be "h6h8".
    TEST_ASSERT_EQUAL_STRING("h6h8", move_str);
}

void test_search_mate_in_2(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    int parse_res = fen_parse("k7/P6p/1K6/8/8/8/8/R7 w - - 0 1", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    SearchLimits limits = {
        .depth = 4,
        .soft_time_limit_ms = 0,
        .hard_time_limit_ms = 0
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);
    TEST_ASSERT_TRUE(result.has_legal_move);

    char move_str[6];
    int str_res = uci_move_to_string(&test_pos, result.best_move, move_str, sizeof(move_str));
    TEST_ASSERT_EQUAL_INT(0, str_res);
    
    TEST_ASSERT_EQUAL_INT('a', move_str[0]);
    TEST_ASSERT_EQUAL_INT('1', move_str[1]);
    TEST_ASSERT_EQUAL_INT('1', move_str[3]);
    TEST_ASSERT_NOT_EQUAL_INT('a', move_str[2]);
    TEST_ASSERT_NOT_EQUAL_INT('b', move_str[2]);
}

void test_search_stalemate(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    // Standard stalemate position (Black to move, King on A8, White Pawn on A7, White King on B6)
    int parse_res = fen_parse("k7/P7/1K6/8/8/8/8/8 b - - 0 1", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    SearchLimits limits = {
        .depth = 2,
        .soft_time_limit_ms = 0,
        .hard_time_limit_ms = 0
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);
    TEST_ASSERT_FALSE(result.has_legal_move);
}

void test_search_time_limit(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    int parse_res = fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    // Set a very deep depth but a short time limit
    SearchLimits limits = {
        .depth = 16,
        .soft_time_limit_ms = 20,
        .hard_time_limit_ms = 40
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);
    TEST_ASSERT_TRUE(result.has_legal_move);
}

void test_search_hash_size(void)
{
    int res = search_set_hash_size_mb(32);
    TEST_ASSERT_EQUAL_INT(0, res);

    // Set back to default
    res = search_set_hash_size_mb(16);
    TEST_ASSERT_EQUAL_INT(0, res);
}

void test_search_repetition(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    // A simple position with only kings
    int parse_res = fen_parse("k7/8/8/8/8/8/8/K7 w - - 0 1", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    SearchLimits limits = {
        .depth = 6,
        .soft_time_limit_ms = 0,
        .hard_time_limit_ms = 0
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);
    TEST_ASSERT_TRUE(result.has_legal_move);
}

void test_search_mate_in_4(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    int parse_res = fen_parse("r1bqk1nr/pp1p2bp/4n3/2p1Npp1/5P2/2N1P1PP/PPP5/1RBQKB1R w Kkq - 0 10", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    SearchLimits limits = {
        .depth = 8,
        .soft_time_limit_ms = 0,
        .hard_time_limit_ms = 0
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);
    TEST_ASSERT_TRUE(result.has_legal_move);
    TEST_ASSERT_TRUE(result.score > MATE_SCORE - 100);
}

void test_search_mate_in_5(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    int parse_res = fen_parse("1Q5R/p1p2P2/3N4/p2k4/2p5/K7/2bP4/R7 w - - 0 1", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    SearchLimits limits = {
        .depth = 9,
        .soft_time_limit_ms = 0,
        .hard_time_limit_ms = 0
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);
    TEST_ASSERT_TRUE(result.has_legal_move);
    TEST_ASSERT_TRUE(result.score > MATE_SCORE - 100);
}

int main(void)
{
    bitboard_init();

    UNITY_BEGIN();

    RUN_TEST(test_search_mate_in_1);
    RUN_TEST(test_search_mate_in_2);
    RUN_TEST(test_search_stalemate);
    RUN_TEST(test_search_time_limit);
    RUN_TEST(test_search_hash_size);
    RUN_TEST(test_search_repetition);
    RUN_TEST(test_search_mate_in_4);
    RUN_TEST(test_search_mate_in_5);
    RUN_TEST(test_search_mate_in_6);

    return UNITY_END();
}
