#include "unity.h"
#include "boards.h"

static Board board;

void setUp(void)
{
    board_startpos(&board);
}

void tearDown(void) { }

/* ── piece-type extraction ───────────────────────────────────────────── */
void test_PIECE_TYPE_returns_correct_type(void)
{
    /* white pieces */
    TEST_ASSERT_EQUAL_UINT8(PAWN,   PIECE_TYPE(W_PAWN));
    TEST_ASSERT_EQUAL_UINT8(KNIGHT, PIECE_TYPE(W_KNIGHT));
    TEST_ASSERT_EQUAL_UINT8(BISHOP, PIECE_TYPE(W_BISHOP));
    TEST_ASSERT_EQUAL_UINT8(ROOK,   PIECE_TYPE(W_ROOK));
    TEST_ASSERT_EQUAL_UINT8(QUEEN,  PIECE_TYPE(W_QUEEN));
    TEST_ASSERT_EQUAL_UINT8(KING,   PIECE_TYPE(W_KING));

    /* black pieces — colour bit must not affect type */
    TEST_ASSERT_EQUAL_UINT8(PAWN,   PIECE_TYPE(B_PAWN));
    TEST_ASSERT_EQUAL_UINT8(KNIGHT, PIECE_TYPE(B_KNIGHT));
    TEST_ASSERT_EQUAL_UINT8(BISHOP, PIECE_TYPE(B_BISHOP));
    TEST_ASSERT_EQUAL_UINT8(ROOK,   PIECE_TYPE(B_ROOK));
    TEST_ASSERT_EQUAL_UINT8(QUEEN,  PIECE_TYPE(B_QUEEN));
    TEST_ASSERT_EQUAL_UINT8(KING,   PIECE_TYPE(B_KING));

    TEST_ASSERT_EQUAL_UINT8(NONE, PIECE_TYPE(EMPTY));
}

/* ── colour extraction ───────────────────────────────────────────────── */
void test_PIECE_COLOR_returns_correct_colour(void)
{
    TEST_ASSERT_EQUAL_UINT8(WHITE, PIECE_COLOR(W_PAWN));
    TEST_ASSERT_EQUAL_UINT8(WHITE, PIECE_COLOR(W_KNIGHT));
    TEST_ASSERT_EQUAL_UINT8(WHITE, PIECE_COLOR(W_BISHOP));
    TEST_ASSERT_EQUAL_UINT8(WHITE, PIECE_COLOR(W_ROOK));
    TEST_ASSERT_EQUAL_UINT8(WHITE, PIECE_COLOR(W_QUEEN));
    TEST_ASSERT_EQUAL_UINT8(WHITE, PIECE_COLOR(W_KING));

    TEST_ASSERT_EQUAL_UINT8(BLACK, PIECE_COLOR(B_PAWN));
    TEST_ASSERT_EQUAL_UINT8(BLACK, PIECE_COLOR(B_KNIGHT));
    TEST_ASSERT_EQUAL_UINT8(BLACK, PIECE_COLOR(B_BISHOP));
    TEST_ASSERT_EQUAL_UINT8(BLACK, PIECE_COLOR(B_ROOK));
    TEST_ASSERT_EQUAL_UINT8(BLACK, PIECE_COLOR(B_QUEEN));
    TEST_ASSERT_EQUAL_UINT8(BLACK, PIECE_COLOR(B_KING));

    TEST_ASSERT_EQUAL_UINT8(WHITE, PIECE_COLOR(EMPTY));
}

/* ── MAKE_PIECE ──────────────────────────────────────────────────────── */
void test_MAKE_PIECE_roundtrips(void)
{
    /* white pieces always have colour bit 0 */
    for (uint8_t t = PAWN; t <= KING; t++) {
        uint8_t p = MAKE_PIECE(WHITE, t);
        TEST_ASSERT_EQUAL_UINT8(WHITE, PIECE_COLOR(p));
        TEST_ASSERT_EQUAL_UINT8(t,     PIECE_TYPE(p));
    }

    /* black pieces always have colour bit 3 set */
    for (uint8_t t = PAWN; t <= KING; t++) {
        uint8_t p = MAKE_PIECE(BLACK, t);
        TEST_ASSERT_EQUAL_UINT8(BLACK, PIECE_COLOR(p));
        TEST_ASSERT_EQUAL_UINT8(t,     PIECE_TYPE(p));
    }
}

/* ── starting position – pawns ───────────────────────────────────────── */
void test_startpos_white_pawns_on_rank2(void)
{
    for (Square sq = A2; sq <= H2; sq++) {
        TEST_ASSERT_EQUAL_UINT8(W_PAWN, board.squares[sq]);
    }
}

void test_startpos_black_pawns_on_rank7(void)
{
    for (Square sq = A7; sq <= H7; sq++) {
        TEST_ASSERT_EQUAL_UINT8(B_PAWN, board.squares[sq]);
    }
}

/* ── starting position – pieces ──────────────────────────────────────── */
void test_startpos_white_back_rank(void)
{
    TEST_ASSERT_EQUAL_UINT8(W_ROOK,   board.squares[A1]);
    TEST_ASSERT_EQUAL_UINT8(W_KNIGHT, board.squares[B1]);
    TEST_ASSERT_EQUAL_UINT8(W_BISHOP, board.squares[C1]);
    TEST_ASSERT_EQUAL_UINT8(W_QUEEN,  board.squares[D1]);
    TEST_ASSERT_EQUAL_UINT8(W_KING,   board.squares[E1]);
    TEST_ASSERT_EQUAL_UINT8(W_BISHOP, board.squares[F1]);
    TEST_ASSERT_EQUAL_UINT8(W_KNIGHT, board.squares[G1]);
    TEST_ASSERT_EQUAL_UINT8(W_ROOK,   board.squares[H1]);
}

void test_startpos_black_back_rank(void)
{
    TEST_ASSERT_EQUAL_UINT8(B_ROOK,   board.squares[A8]);
    TEST_ASSERT_EQUAL_UINT8(B_KNIGHT, board.squares[B8]);
    TEST_ASSERT_EQUAL_UINT8(B_BISHOP, board.squares[C8]);
    TEST_ASSERT_EQUAL_UINT8(B_QUEEN,  board.squares[D8]);
    TEST_ASSERT_EQUAL_UINT8(B_KING,   board.squares[E8]);
    TEST_ASSERT_EQUAL_UINT8(B_BISHOP, board.squares[F8]);
    TEST_ASSERT_EQUAL_UINT8(B_KNIGHT, board.squares[G8]);
    TEST_ASSERT_EQUAL_UINT8(B_ROOK,   board.squares[H8]);
}

/* ── emptiness ───────────────────────────────────────────────────────── */
void test_startpos_ranks_3456_are_empty(void)
{
    for (Square sq = A3; sq <= H6; sq++) {
        TEST_ASSERT_EQUAL_UINT8(EMPTY, board.squares[sq]);
    }
}

/* ── square count ────────────────────────────────────────────────────── */
void test_square_count_is_64(void)
{
    TEST_ASSERT_EQUAL_INT(64, SQUARE_NB);
}

/* ── main (Unity runner) ──────────────────────────────────────────────── */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_PIECE_TYPE_returns_correct_type);
    RUN_TEST(test_PIECE_COLOR_returns_correct_colour);
    RUN_TEST(test_MAKE_PIECE_roundtrips);
    RUN_TEST(test_startpos_white_pawns_on_rank2);
    RUN_TEST(test_startpos_black_pawns_on_rank7);
    RUN_TEST(test_startpos_white_back_rank);
    RUN_TEST(test_startpos_black_back_rank);
    RUN_TEST(test_startpos_ranks_3456_are_empty);
    RUN_TEST(test_square_count_is_64);

    return UNITY_END();
}