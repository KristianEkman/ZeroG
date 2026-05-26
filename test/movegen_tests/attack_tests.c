#include "test_helpers.h"

void test_e4_attacked_by_black_knight_on_f6(void)
{
    pos.board[F6] = B_KNIGHT;
    pos.pieces[1][KNIGHT] |= (1ULL << F6);
    pos.occ[1] |= (1ULL << F6);
    pos.occAll = pos.occ[0] | pos.occ[1];

    TEST_ASSERT(is_square_attacked(&pos, E4, BLACK));
    TEST_ASSERT_FALSE(is_square_attacked(&pos, E4, WHITE));
}

void test_d2_attacked_by_white_queen_on_d1(void)
{
    TEST_ASSERT(is_square_attacked(&pos, D2, WHITE));
    TEST_ASSERT_FALSE(is_square_attacked(&pos, D2, BLACK));
}

void test_f7_attacked_by_white_bishop_on_c4(void)
{
    /* Build a minimal clean position: only a white bishop on c4 */
    memset(&pos, 0, sizeof(pos));
    pos.board[C4] = W_BISHOP;
    pos.pieces[0][BISHOP] = 1ULL << C4;
    pos.occ[0] = 1ULL << C4;
    pos.occAll = 1ULL << C4;

    TEST_ASSERT(is_square_attacked(&pos, F7, WHITE));
}

void test_g2_attacked_by_black_pawn_on_h3(void)
{
    pos.board[H3] = B_PAWN;
    pos.pieces[1][PAWN] |= (1ULL << H3);
    pos.occ[1] |= (1ULL << H3);
    pos.occAll = pos.occ[0] | pos.occ[1];

    TEST_ASSERT(is_square_attacked(&pos, G2, BLACK));
}
