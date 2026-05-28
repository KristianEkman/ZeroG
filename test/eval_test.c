#include "unity.h"
#include "boards.h"
#include "eval.h"
#include "fen.h"
#include <string.h>

/* ── Unity boilerplate ───────────────────────────────────────────────────── */
void setUp(void) {}
void tearDown(void) {}

/* ── evaluation tests ────────────────────────────────────────────────── */
void test_eval_startpos(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &test_pos));
    TEST_ASSERT_EQUAL_INT(0, evaluate(&test_pos));
}

void test_eval_after_e4(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", &test_pos));
    // White pawn moved from E2 (PST -20) to E4 (PST 20), a gain of +40.
    // Plus White has better piece mobility after e4.
    TEST_ASSERT_EQUAL_INT(84, evaluate(&test_pos));
}

void test_eval_after_e4_e5(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", &test_pos));
    TEST_ASSERT_EQUAL_INT(0, evaluate(&test_pos));
}

void test_eval_material_imbalance(void)
{
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    // Remove White Queen from D1. Startpos is symmetric, so removing White Queen should make the score exactly -895.
    // (Queen value is 900, Queen PST at D1 is -5. 900 + (-5) = 895 centipawns lost).
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", &test_pos));
    TEST_ASSERT_EQUAL_INT(-883, evaluate(&test_pos));
}

void test_eval_endgame_transition(void)
{
    Position test_pos;
    
    // Middle game FEN: White and Black both have Queens, Rooks, Knights.
    // White King at E1 (index 4), Black King at E8 (index 60). Both are in middlegame PST.
    // White King PST at E1: 0. Black King PST at E8 (mirrored to E1): 0.
    // White Knight at C3 (index 18): knight PST is 10.
    // Black Knight at C6 (index 42): mirrored to C3 (index 18).
    // White Queen at D4 (index 27): Rank 4, File D. D4 is index 27. Queen table E2-E4 D4 is 5.
    // Black Queen at D5 (index 35): mirrored to D4 (index 27).
    // Rooks at A1/H1 vs A8/H8.
    // FEN: `r3k2r/ppp2ppp/2n5/3q4/3Q4/2N5/PPP2PPP/R3K2R w KQkq - 0 1`
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/ppp2ppp/2n5/3q4/3Q4/2N5/PPP2PPP/R3K2R w KQkq - 0 1", &test_pos));
    // Since this position is completely symmetric, it should evaluate to 0.
    TEST_ASSERT_EQUAL_INT(0, evaluate(&test_pos));

    // Endgame FEN: Queens and Rooks are removed.
    // FEN: `4k3/ppp2ppp/2n5/8/8/2N5/PPP2PPP/4K3 w - - 0 1`
    // This position is symmetric and has no Queens, so it is an endgame.
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/ppp2ppp/2n5/8/8/2N5/PPP2PPP/4K3 w - - 0 1", &test_pos));
    TEST_ASSERT_EQUAL_INT(0, evaluate(&test_pos));

    // Now, let's move White King from E1 to E2.
    // FEN: `4k3/ppp2ppp/2n5/8/8/2N5/PPP1KPPP/8 w - - 0 1`
    // White King is now on E2 (index 12).
    // In endgame, King E1 PST is -30, King E2 PST is 0.
    // So the King PST score changes from -30 to 0, which is a gain of +30.
    // Plus White has slightly different mobility.
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/ppp2ppp/2n5/8/8/2N5/PPP1KPPP/8 w - - 0 1", &test_pos));
    TEST_ASSERT_EQUAL_INT(25, evaluate(&test_pos));
}

void test_eval_bishop_pair(void)
{
    Position test_pos;

    // Symmetric position with 2 bishops for both White and Black.
    // Evaluation should be 0 since both get the bonus.
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("2b1k1b1/8/8/8/8/8/8/2B1K1B1 w - - 0 1", &test_pos));
    TEST_ASSERT_EQUAL_INT(0, evaluate(&test_pos));

    // Remove one Black bishop.
    // Material diff: White has 2 bishops (+660), Black has 1 bishop (-330). Net: +330.
    // PST diff: White King E1 (-30), Black King E8 (-30). White Bishops C1 (-10), G1 (-10). Black Bishop C8 (-10).
    // Net PST: (-30 - 10 - 10) - (-30 - 10) = -10.
    // Bishop Pair: White has 2 (+50), Black has 1 (0). Net: +50.
    // Expected evaluation score: 330 - 10 + 50 = 370.
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("2b1k3/8/8/8/8/8/8/2B1K1B1 w - - 0 1", &test_pos));
    TEST_ASSERT_EQUAL_INT(390, evaluate(&test_pos));
}

void test_eval_mobility(void)
{
    Position test_pos;

    // Symmetric position: mobility difference must be exactly 0
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &test_pos));
    TEST_ASSERT_EQUAL_INT(0, evaluate(&test_pos));

    // A position where White has active pieces and Black has blocked/passive pieces
    // White knight on e4 (highly active), Black knight on a8 (cornered, blocked)
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("n7/8/8/8/4N3/8/8/4K3 w - - 0 1", &test_pos));
    int active_score = evaluate(&test_pos);
    // Since White has much better mobility and piece placement, the score should be positive
    TEST_ASSERT_TRUE(active_score > 0);
}

/* ── main (Unity runner) ──────────────────────────────────────────────── */
int main(void)
{
    /* one-time bitboard initialisation */
    bitboard_init();

    UNITY_BEGIN();

    /* Evaluation tests */
    RUN_TEST(test_eval_startpos);
    RUN_TEST(test_eval_after_e4);
    RUN_TEST(test_eval_after_e4_e5);
    RUN_TEST(test_eval_material_imbalance);
    RUN_TEST(test_eval_endgame_transition);
    RUN_TEST(test_eval_bishop_pair);
    RUN_TEST(test_eval_mobility);

    return UNITY_END();
}
