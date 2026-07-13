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
    // White pawn moved from E2 to E4.
    // White has better piece mobility after e4, though PST changes.
    TEST_ASSERT_TRUE(evaluate(&test_pos) > 0);
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
    // Remove White Queen from D1.
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", &test_pos));
    TEST_ASSERT_TRUE(evaluate(&test_pos) < -500);
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
    // White King is now on E2.
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/ppp2ppp/2n5/8/8/2N5/PPP1KPPP/8 w - - 0 1", &test_pos));
    TEST_ASSERT_TRUE(evaluate(&test_pos) > 0);
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
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("2b1k3/8/8/8/8/8/8/2B1K1B1 w - - 0 1", &test_pos));
    TEST_ASSERT_TRUE(evaluate(&test_pos) > 100);
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

void test_eval_passed_pawns(void)
{
    Position test_pos;

    // 1. Test basic passed pawn presence
    // Symmetric position with Kings:
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/8/8/8/8/4K3 w - - 0 1", &test_pos));
    int base_kings = evaluate(&test_pos);

    // Now add White pawn on E5: FEN: 4k3/8/8/4P3/8/8/8/4K3 w - - 0 1
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/4P3/8/8/8/4K3 w - - 0 1", &test_pos));
    int with_pawn = evaluate(&test_pos);
    TEST_ASSERT_TRUE(with_pawn - base_kings > 100);

    // 2. Test blocked passed pawn (30% reduction)
    // Compare pawn unblocked (Black King on C6) vs pawn blocked (Black King on E6).
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/8/2k5/4P3/8/8/8/4K3 w - - 0 1", &test_pos));
    int score_unblocked = evaluate(&test_pos);

    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/8/4k3/4P3/8/8/8/4K3 w - - 0 1", &test_pos));
    int score_blocked = evaluate(&test_pos);
    TEST_ASSERT_TRUE(score_unblocked > score_blocked);

    // 3. Test protected passed pawn
    // Place a White pawn on D4 to protect E5:
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/4P3/3P4/8/8/4K3 w - - 0 1", &test_pos));
    int score_protected = evaluate(&test_pos);

    // Place a White pawn on C4 (does not protect E5):
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/4P3/2P5/8/8/4K3 w - - 0 1", &test_pos));
    int score_unprotected = evaluate(&test_pos);
    TEST_ASSERT_TRUE(score_protected > score_unprotected);

    // 4. Test rook behind passed pawn
    // Rook on E3 (behind E5):
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/4P3/8/4R3/8/4K3 w - - 0 1", &test_pos));
    int score_rook_behind = evaluate(&test_pos);

    // Rook on A3 (not behind E5):
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/4P3/8/R7/8/4K3 w - - 0 1", &test_pos));
    int score_rook_side = evaluate(&test_pos);
    TEST_ASSERT_TRUE(score_rook_behind > score_rook_side);
}

void test_eval_isolated_pawns(void)
{
    Position pos_connected;
    Position pos_isolated;

    // Case A: Connected White pawns on D3 and E3
    memset(&pos_connected, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/8/8/3PP3/8/4K3 w - - 0 1", &pos_connected));
    int score_connected = evaluate(&pos_connected);

    // Case B: Isolated White pawns on C3 and E3
    memset(&pos_isolated, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/8/8/2P1P3/8/4K3 w - - 0 1", &pos_isolated));
    int score_isolated = evaluate(&pos_isolated);

    // Connected pawns should evaluate higher than isolated pawns
    TEST_ASSERT_TRUE(score_connected > score_isolated);
}
void test_eval_doubled_pawns(void)
{
    Position pos_single;
    Position pos_doubled;
    Position pos_tripled;

    // Case A: 3 pawns, single and connected (A2, B2, C2) - healthy
    memset(&pos_single, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/8/8/8/PPP5/4K3 w - - 0 1", &pos_single));
    int score_single = evaluate(&pos_single);

    // Case B: 3 pawns, doubled on A-file (A2, A3, H4)
    memset(&pos_doubled, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/8/7P/P7/P7/4K3 w - - 0 1", &pos_doubled));
    int score_doubled = evaluate(&pos_doubled);

    // Case C: 3 pawns, tripled on A-file (A2, A3, A4)
    memset(&pos_tripled, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/8/P7/P7/P7/4K3 w - - 0 1", &pos_tripled));
    int score_tripled = evaluate(&pos_tripled);

    // Single pawns should evaluate higher than doubled pawns, which evaluate higher than tripled pawns
    TEST_ASSERT_TRUE(score_single > score_doubled);
    TEST_ASSERT_TRUE(score_doubled > score_tripled);
}

void test_eval_rook_open_files(void)
{
    Position pos_symmetric;
    Position pos_open;
    Position pos_semi_open;
    Position pos_closed;

    // 1. Symmetric position: both sides have a rook on an open file (D-file)
    // FEN: 3rk3/ppp2ppp/8/8/8/8/PPP2PPP/3RK3 w - - 0 1
    // D-file is completely open. Both sides have identical setups. Score must be exactly 0.
    memset(&pos_symmetric, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("3rk3/ppp2ppp/8/8/8/8/PPP2PPP/3RK3 w - - 0 1", &pos_symmetric));
    TEST_ASSERT_EQUAL_INT(0, evaluate(&pos_symmetric));

    // 2. Rook on Open File (D1)
    // FEN: 4k3/ppp1pppp/8/8/8/8/PPP2PPP/3RK3 w - - 0 1
    // D-file has no White or Black pawns -> Open file for the Rook on D1.
    memset(&pos_open, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/ppp1pppp/8/8/8/8/PPP2PPP/3RK3 w - - 0 1", &pos_open));
    int score_open = evaluate(&pos_open);

    // 3. Rook on Semi-Open File (E1)
    // FEN: 4k3/ppp1pppp/8/8/8/8/PPP2PPP/3KR3 w - - 0 1
    // E-file has no White pawns, but has Black pawn on E7 -> Semi-open file for the Rook on E1.
    // King is on D1 (same PST score as E1 in endgame).
    memset(&pos_semi_open, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/ppp1pppp/8/8/8/8/PPP2PPP/3KR3 w - - 0 1", &pos_semi_open));
    int score_semi_open = evaluate(&pos_semi_open);

    // 4. Rook on Closed File (F1)
    // FEN: 4k3/ppp1pppp/8/8/8/8/PPP2PPP/4KR2 w - - 0 1
    // F-file has White pawn on F2 -> Closed file for the Rook on F1.
    memset(&pos_closed, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/ppp1pppp/8/8/8/8/PPP2PPP/4KR2 w - - 0 1", &pos_closed));
    int score_closed = evaluate(&pos_closed);

    // Rook on open file should evaluate higher than rook on semi-open file
    TEST_ASSERT_TRUE(score_open > score_semi_open);

    // Rook on semi-open file should evaluate higher than rook on closed file
    TEST_ASSERT_TRUE(score_semi_open > score_closed);

    // Rook on open file should evaluate higher than rook on closed file
    TEST_ASSERT_TRUE(score_open > score_closed);
}

void test_eval_king_safety(void)
{
    Position pos_full_shield;
    Position pos_missing_pawn;
    Position pos_advanced_pawn;
    Position pos_one_attacker;
    Position pos_two_attackers;

    // 1. Pawn Shield Tests
    // Symmetric starting position with kings castled: FEN with intact pawn shields for both
    memset(&pos_full_shield, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("5rk1/ppp2ppp/8/8/8/8/PPP2PPP/5RK1 w - - 0 1", &pos_full_shield));
    int score_full = evaluate(&pos_full_shield);
    TEST_ASSERT_EQUAL_INT(0, score_full); // symmetric should be 0

    // White F2 pawn is missing (open file in front of king)
    memset(&pos_missing_pawn, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("5rk1/ppp2ppp/8/8/8/8/P1P2PPP/5RK1 w - - 0 1", &pos_missing_pawn));
    int score_missing = evaluate(&pos_missing_pawn);
    // Missing shield pawn must result in a penalty, so score < 0
    TEST_ASSERT_TRUE(score_missing < 0);

    // White F2 pawn advanced to F3 (weaker shield than F2 but better than missing)
    memset(&pos_advanced_pawn, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("5rk1/ppp2ppp/8/8/8/5P2/PP1P2PP/5RK1 w - - 0 1", &pos_advanced_pawn));
    int score_advanced = evaluate(&pos_advanced_pawn);
    // Shield with advanced pawn should be better than missing, but worse than full shield
    TEST_ASSERT_TRUE(score_advanced < 0);
    TEST_ASSERT_TRUE(score_missing < score_advanced);

    // 2. King Attacks Tests
    // One attacker (Black Queen on G6, Black Knight on D5 - too far to attack G1 zone)
    memset(&pos_one_attacker, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("6k1/ppp2ppp/6q1/3n4/8/8/PPP2PPP/5RK1 w - - 0 1", &pos_one_attacker));
    int score_one = evaluate(&pos_one_attacker);

    // Two attackers (Black Queen on G6, Black Knight on E3 - attacks F1/G2 in G1 zone)
    memset(&pos_two_attackers, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("6k1/ppp2ppp/6q1/8/8/4n3/PPP2PPP/5RK1 w - - 0 1", &pos_two_attackers));
    int score_two = evaluate(&pos_two_attackers);

    // Position with two attackers should have a lower evaluation (more negative) than one attacker due to king attacks penalty
    // Subtracting Knight value from E3 vs D5 first (which is comparable or identical PST-wise)
    // Knight at E3 PST is 5, Knight at D5 PST is 20. So Knight at E3 is slightly worse for Black, which would favor White.
    // However, the King safety penalty for 2 attackers is much stronger, so score_two must be lower than score_one.
    TEST_ASSERT_TRUE(score_two < score_one);
}

void test_eval_nn(void)
{
    if (!eval_nn)
    {
        TEST_IGNORE_MESSAGE("NN weights not loaded, skipping NN test");
    }
    use_nn = true;
    Position test_pos;
    memset(&test_pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &test_pos));
    int score = evaluate(&test_pos);
    // Symmetric startpos should evaluate close to 0 (within 100cp)
    TEST_ASSERT_INT_WITHIN(100, 0, score);
    use_nn = false;
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
    RUN_TEST(test_eval_passed_pawns);
    RUN_TEST(test_eval_isolated_pawns);
    RUN_TEST(test_eval_doubled_pawns);
    RUN_TEST(test_eval_rook_open_files);
    RUN_TEST(test_eval_king_safety);
    RUN_TEST(test_eval_nn);

    int result = UNITY_END();
    eval_free();
    return result;
}
