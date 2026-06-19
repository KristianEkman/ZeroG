#include "unity.h"
#include "boards.h"
#include "fen.h"
#include "movegen.h"
#include "search/search.h"
#include "eval.h"
#include "search/time_control.h"
#include "uci/uci.h"
#include "uci/uci_internal.h"
#include "see.h"
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

void test_search_mate_in_4b(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    int parse_res = fen_parse("7R/pQ3P2/2pN4/p2k4/2p5/K7/2bP4/R7 w - - 0 2", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    SearchLimits limits = {
        .depth = 10,
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

void test_search_no_illegal_castle_in_check(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    // FEN: White to move. White is in checkmate, but has pseudo-legal castling e1g1.
    // G1 is safe, but e1 (current king square) and f1 (intermediate square) are attacked by the Black queen on e2.
    // The queen on e2 is protected by the pawn on d3, so Kxe2 is illegal.
    // Search must recognize this is checkmate and has no legal moves.
    int parse_res = fen_parse("3rkr2/8/8/8/8/3p4/4q3/R3K2R w KQ - 0 1", &test_pos);
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
    
    // The engine should detect that White is checkmated and has no legal moves.
    TEST_ASSERT_FALSE(result.has_legal_move);
}

void test_search_pv_promotion_suffix(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    // FEN: White pawn on a6, Black king on h8.
    // White wants to promote pawn: a6a7 -> ... -> a7a8q.
    int parse_res = fen_parse("7k/8/P7/8/8/8/8/K7 w - - 0 1", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    // Open a temporary log file
    FILE *log_file = fopen("builds/test_pv.txt", "w+");
    TEST_ASSERT_NOT_NULL(log_file);
    FILE *prev_log = search_set_log_output(log_file);

    SearchLimits limits = {
        .depth = 3,
        .soft_time_limit_ms = 0,
        .hard_time_limit_ms = 0
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);

    search_set_log_output(prev_log);
    
    // Rewind and read the file
    fseek(log_file, 0, SEEK_SET);
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, log_file);
    buf[n] = '\0';
    fclose(log_file);

    // Check if the output contains the illegal move "a7a8" without suffix
    // and make sure it has the correct "a7a8q" suffix.
    TEST_ASSERT_NOT_NULL(strstr(buf, "a7a8q"));
    TEST_ASSERT_NULL(strstr(buf, "a7a8 "));
    TEST_ASSERT_NULL(strstr(buf, "a7a8\n"));
}

void test_time_control_one_legal_move(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    int parse_res = fen_parse("kR6/P7/2K5/8/8/8/8/8 b - - 0 1", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    SearchLimits limits = {
        .depth = 12,
        .remaining_time_ms = 1000,
        .increment_ms = 0,
        .movestogo = 0,
        .is_time_controlled = 1
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);
    TEST_ASSERT_TRUE(result.has_legal_move);

    // Verify Kxa7 is indeed the move (a8a7)
    char move_str[6];
    int str_res = uci_move_to_string(&test_pos, result.best_move, move_str, sizeof(move_str));
    TEST_ASSERT_EQUAL_INT(0, str_res);
    TEST_ASSERT_EQUAL_STRING("a8a7", move_str);

    // Iterative deepening should stop immediately because there's only 1 legal move.
    // Node count should be very small (typically < 10 nodes).
    TEST_ASSERT_TRUE(result.node_count < 100);
}

void test_time_control_calculations(void)
{
    Position mid_pos;
    memset(&mid_pos, 0, sizeof(Position));
    fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &mid_pos);

    Position end_pos;
    memset(&end_pos, 0, sizeof(Position));
    fen_parse("k7/8/8/8/8/8/8/K7 w - - 0 1", &end_pos);

    TimeControl tc_mid, tc_end;
    time_control_init(&tc_mid, &mid_pos, 40000, 0, 0, 20);
    time_control_init(&tc_end, &end_pos, 40000, 0, 0, 5);

    // Middlegame should budget for more moves (slower play), so soft limit is smaller.
    // Endgame should budget for fewer moves (faster play), so soft limit is larger.
    TEST_ASSERT_TRUE(tc_mid.soft_limit_ms < tc_end.soft_limit_ms);

    // Test movestogo override
    TimeControl tc_override;
    time_control_init(&tc_override, &mid_pos, 40000, 0, 10, 20);
    // With movestogo=10, we budget for 10 moves, so soft limit is larger than tc_mid (moves_to_go=40)
    TEST_ASSERT_TRUE(tc_override.soft_limit_ms > tc_mid.soft_limit_ms);
}

void test_search_history_heuristic(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    int parse_res = fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &test_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    SearchLimits limits = {
        .depth = 5,
        .soft_time_limit_ms = 0,
        .hard_time_limit_ms = 0
    };
    SearchResult result;
    memset(&result, 0, sizeof(SearchResult));

    int search_res = search_best_move_with_limits(&test_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);

    // Let's iterate over all possible from/to squares and find if any has a positive history score.
    int found_history_score = 0;
    for (int side = 0; side < 2; side++) {
        Color color = (side == 0) ? WHITE : BLACK;
        for (int from = 0; from < 64; from++) {
            for (int to = 0; to < 64; to++) {
                int score = search_get_history_score(color, from, to);
                if (score > 0) {
                    found_history_score = 1;
                }
            }
        }
    }
    TEST_ASSERT_TRUE(found_history_score);

    // Let's verify it gets reset. We search a checkmate position (no legal moves).
    // The search should immediately return, and the history table should be reset to 0.
    Position mate_pos;
    memset(&mate_pos, 0, sizeof(Position));
    parse_res = fen_parse("3rkr2/8/8/8/8/3p4/4q3/R3K2R w KQ - 0 1", &mate_pos);
    TEST_ASSERT_EQUAL_INT(0, parse_res);

    limits.depth = 1;
    memset(&result, 0, sizeof(SearchResult));
    search_res = search_best_move_with_limits(&mate_pos, &limits, &result);
    TEST_ASSERT_EQUAL_INT(0, search_res);

    // Now all history scores should be 0.
    int has_history_after_reset = 0;
    for (int side = 0; side < 2; side++) {
        Color color = (side == 0) ? WHITE : BLACK;
        for (int from = 0; from < 64; from++) {
            for (int to = 0; to < 64; to++) {
                if (search_get_history_score(color, from, to) > 0) {
                    has_history_after_reset = 1;
                }
            }
        }
    }
    TEST_ASSERT_FALSE(has_history_after_reset);
}

void test_search_options(void)
{
    // Test direct search setters constraints
    TEST_ASSERT_EQUAL_INT(-1, search_set_lmr_base(-1));
    TEST_ASSERT_EQUAL_INT(-1, search_set_lmr_base(21));
    TEST_ASSERT_EQUAL_INT(0, search_set_lmr_base(0));
    TEST_ASSERT_EQUAL_INT(0, search_set_lmr_base(5));
    TEST_ASSERT_EQUAL_INT(0, search_set_lmr_base(20));

    TEST_ASSERT_EQUAL_INT(-1, search_set_futility_margin(-1));
    TEST_ASSERT_EQUAL_INT(-1, search_set_futility_margin(501));
    TEST_ASSERT_EQUAL_INT(0, search_set_futility_margin(0));
    TEST_ASSERT_EQUAL_INT(0, search_set_futility_margin(100));
    TEST_ASSERT_EQUAL_INT(0, search_set_futility_margin(500));

    TEST_ASSERT_EQUAL_INT(-1, search_set_rfp_margin(-1));
    TEST_ASSERT_EQUAL_INT(-1, search_set_rfp_margin(301));
    TEST_ASSERT_EQUAL_INT(0, search_set_rfp_margin(0));
    TEST_ASSERT_EQUAL_INT(0, search_set_rfp_margin(120));
    TEST_ASSERT_EQUAL_INT(0, search_set_rfp_margin(300));

    TEST_ASSERT_EQUAL_INT(-1, search_set_nmp_min_depth(0));
    TEST_ASSERT_EQUAL_INT(-1, search_set_nmp_min_depth(11));
    TEST_ASSERT_EQUAL_INT(0, search_set_nmp_min_depth(1));
    TEST_ASSERT_EQUAL_INT(0, search_set_nmp_min_depth(3));
    TEST_ASSERT_EQUAL_INT(0, search_set_nmp_min_depth(10));

    TEST_ASSERT_EQUAL_INT(-1, search_set_singular_margin(-1));
    TEST_ASSERT_EQUAL_INT(-1, search_set_singular_margin(11));
    TEST_ASSERT_EQUAL_INT(0, search_set_singular_margin(0));
    TEST_ASSERT_EQUAL_INT(0, search_set_singular_margin(2));
    TEST_ASSERT_EQUAL_INT(0, search_set_singular_margin(10));

    TEST_ASSERT_EQUAL_INT(-1, search_set_aspiration_window(4));
    TEST_ASSERT_EQUAL_INT(-1, search_set_aspiration_window(201));
    TEST_ASSERT_EQUAL_INT(0, search_set_aspiration_window(5));
    TEST_ASSERT_EQUAL_INT(0, search_set_aspiration_window(40));
    TEST_ASSERT_EQUAL_INT(0, search_set_aspiration_window(200));

    TEST_ASSERT_EQUAL_INT(-1, search_set_lmr_min_depth(0));
    TEST_ASSERT_EQUAL_INT(-1, search_set_lmr_min_depth(16));
    TEST_ASSERT_EQUAL_INT(0, search_set_lmr_min_depth(1));
    TEST_ASSERT_EQUAL_INT(0, search_set_lmr_min_depth(5));
    TEST_ASSERT_EQUAL_INT(0, search_set_lmr_min_depth(15));

    TEST_ASSERT_EQUAL_INT(-1, search_set_futility_max_depth(0));
    TEST_ASSERT_EQUAL_INT(-1, search_set_futility_max_depth(6));
    TEST_ASSERT_EQUAL_INT(0, search_set_futility_max_depth(1));
    TEST_ASSERT_EQUAL_INT(0, search_set_futility_max_depth(2));
    TEST_ASSERT_EQUAL_INT(0, search_set_futility_max_depth(5));

    TEST_ASSERT_EQUAL_INT(-1, search_set_lmr_history_divisor(99));
    TEST_ASSERT_EQUAL_INT(-1, search_set_lmr_history_divisor(100001));
    TEST_ASSERT_EQUAL_INT(0, search_set_lmr_history_divisor(100));
    TEST_ASSERT_EQUAL_INT(0, search_set_lmr_history_divisor(2000));
    TEST_ASSERT_EQUAL_INT(0, search_set_lmr_history_divisor(100000));

    // Test parsing helpers
    int val = -999;
    // Valid cases
    TEST_ASSERT_EQUAL_INT(0, uci_parse_spin_option_value("name LMR_Base value 12", "LMR_Base", &val));
    TEST_ASSERT_EQUAL_INT(12, val);

    TEST_ASSERT_EQUAL_INT(0, uci_parse_spin_option_value("name RFP_Margin value 150", "RFP_Margin", &val));
    TEST_ASSERT_EQUAL_INT(150, val);

    TEST_ASSERT_EQUAL_INT(0, uci_parse_spin_option_value("name NMP_Min_Depth value 4", "NMP_Min_Depth", &val));
    TEST_ASSERT_EQUAL_INT(4, val);

    TEST_ASSERT_EQUAL_INT(0, uci_parse_spin_option_value("name Singular_Margin value 3", "Singular_Margin", &val));
    TEST_ASSERT_EQUAL_INT(3, val);

    TEST_ASSERT_EQUAL_INT(0, uci_parse_spin_option_value("  name   Futility_Margin   value   250  ", "Futility_Margin", &val));
    TEST_ASSERT_EQUAL_INT(250, val);

    TEST_ASSERT_EQUAL_INT(0, uci_parse_spin_option_value("name LMR_History_Divisor value 9500", "LMR_History_Divisor", &val));
    TEST_ASSERT_EQUAL_INT(9500, val);

    // Negative option value (even if out of bounds for the search constraint, parser parses it correctly)
    TEST_ASSERT_EQUAL_INT(0, uci_parse_spin_option_value("name Futility_Margin value -10", "Futility_Margin", &val));
    TEST_ASSERT_EQUAL_INT(-10, val);

    // Invalid cases
    TEST_ASSERT_EQUAL_INT(-1, uci_parse_spin_option_value("name LMR_Base value", "LMR_Base", &val));
    TEST_ASSERT_EQUAL_INT(-1, uci_parse_spin_option_value("name LMR_Base value abc", "LMR_Base", &val));
    TEST_ASSERT_EQUAL_INT(-1, uci_parse_spin_option_value("name LMR_Base value 12 extra", "LMR_Base", &val));
    TEST_ASSERT_EQUAL_INT(-1, uci_parse_spin_option_value("name Futility_Margin value 100", "LMR_Base", &val)); // Name mismatch
}

void test_see(void)
{
    Position pos;
    Move m;
    int score;

    // Test 1: Simple undefended capture (White Rook on e1 captures Black Pawn on e4)
    memset(&pos, 0, sizeof(Position));
    fen_parse("8/8/8/8/4p3/8/8/4R1K1 w - - 0 1", &pos);
    m = MOVE_BUILD(E1, E4, 0, MOVE_QUIET);
    score = see(&pos, m);
    TEST_ASSERT_EQUAL_INT(100, score);

    // Test 2: Simple defended capture (losing) (White Knight on f3 captures Black Pawn on d4, defended by Black Pawn on e5)
    memset(&pos, 0, sizeof(Position));
    fen_parse("8/8/8/4p3/3p4/5N2/8/6K1 w - - 0 1", &pos);
    m = MOVE_BUILD(F3, D4, 0, MOVE_QUIET);
    score = see(&pos, m);
    TEST_ASSERT_EQUAL_INT(-200, score);

    // Test 3: Trade (equal exchange) (White Knight on f3 captures Black Bishop on d4, defended by Black Knight on f5)
    memset(&pos, 0, sizeof(Position));
    fen_parse("8/8/8/5n2/3b4/5N2/8/6K1 w - - 0 1", &pos);
    m = MOVE_BUILD(F3, D4, 0, MOVE_QUIET);
    score = see(&pos, m);
    TEST_ASSERT_EQUAL_INT(0, score);

    // Test 4: X-ray attack (White Bishop on b2 captures Black Knight on d4, defended by Black Pawn on e5. White Queen on a1 is X-raying behind Bishop)
    memset(&pos, 0, sizeof(Position));
    fen_parse("8/8/8/4p3/3n4/8/1B6/Q5K1 w - - 0 1", &pos);
    m = MOVE_BUILD(B2, D4, 0, MOVE_QUIET);
    score = see(&pos, m);
    TEST_ASSERT_EQUAL_INT(100, score); // 300 (Knight) - 300 (Bishop) + 100 (Pawn) = 100

    // Test 5: Non-pawn move to en-passant square (White Knight on f4 moves to e6 when en-passant square is e6)
    memset(&pos, 0, sizeof(Position));
    fen_parse("8/8/8/3pp3/4PN2/8/8/4R1K1 w - e6 0 1", &pos);
    m = MOVE_BUILD(F4, E6, 0, MOVE_QUIET);
    score = see(&pos, m);
    TEST_ASSERT_EQUAL_INT(0, score); // Quiet move, should be 0
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
    RUN_TEST(test_search_mate_in_4b);
    RUN_TEST(test_search_no_illegal_castle_in_check);
    RUN_TEST(test_search_pv_promotion_suffix);
    RUN_TEST(test_time_control_one_legal_move);
    RUN_TEST(test_time_control_calculations);
    RUN_TEST(test_search_history_heuristic);
    RUN_TEST(test_search_options);
    RUN_TEST(test_see);

    int result = UNITY_END();
    eval_free();
    return result;
}
