#include "unity.h"
#include "boards.h"
#include "fen.h"
#include <string.h>

static Position pos;

void setUp(void)
{
    /* bitboard_init only needs to be called once, but Unity setUp is fine */
    /* We call position_startpos each time for a fresh board */
    position_startpos(&pos);
}

void tearDown(void) {}

/* ── piece-type extraction ───────────────────────────────────────────── */
void test_PIECE_TYPE_returns_correct_type(void)
{
    /* white pieces */
    TEST_ASSERT_EQUAL_UINT8(PAWN, PIECE_TYPE(W_PAWN));
    TEST_ASSERT_EQUAL_UINT8(KNIGHT, PIECE_TYPE(W_KNIGHT));
    TEST_ASSERT_EQUAL_UINT8(BISHOP, PIECE_TYPE(W_BISHOP));
    TEST_ASSERT_EQUAL_UINT8(ROOK, PIECE_TYPE(W_ROOK));
    TEST_ASSERT_EQUAL_UINT8(QUEEN, PIECE_TYPE(W_QUEEN));
    TEST_ASSERT_EQUAL_UINT8(KING, PIECE_TYPE(W_KING));

    /* black pieces — colour bit must not affect type */
    TEST_ASSERT_EQUAL_UINT8(PAWN, PIECE_TYPE(B_PAWN));
    TEST_ASSERT_EQUAL_UINT8(KNIGHT, PIECE_TYPE(B_KNIGHT));
    TEST_ASSERT_EQUAL_UINT8(BISHOP, PIECE_TYPE(B_BISHOP));
    TEST_ASSERT_EQUAL_UINT8(ROOK, PIECE_TYPE(B_ROOK));
    TEST_ASSERT_EQUAL_UINT8(QUEEN, PIECE_TYPE(B_QUEEN));
    TEST_ASSERT_EQUAL_UINT8(KING, PIECE_TYPE(B_KING));

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
    for (uint8_t t = PAWN; t <= KING; t++)
    {
        uint8_t p = MAKE_PIECE(WHITE, t);
        TEST_ASSERT_EQUAL_UINT8(WHITE, PIECE_COLOR(p));
        TEST_ASSERT_EQUAL_UINT8(t, PIECE_TYPE(p));
    }

    /* black pieces always have colour bit 3 set */
    for (uint8_t t = PAWN; t <= KING; t++)
    {
        uint8_t p = MAKE_PIECE(BLACK, t);
        TEST_ASSERT_EQUAL_UINT8(BLACK, PIECE_COLOR(p));
        TEST_ASSERT_EQUAL_UINT8(t, PIECE_TYPE(p));
    }
}

/* ── starting position – pawns ───────────────────────────────────────── */
void test_startpos_white_pawns_on_rank2(void)
{
    for (Square sq = A2; sq <= H2; sq++)
    {
        TEST_ASSERT_EQUAL_UINT8(W_PAWN, pos.board[sq]);
    }
}

void test_startpos_black_pawns_on_rank7(void)
{
    for (Square sq = A7; sq <= H7; sq++)
    {
        TEST_ASSERT_EQUAL_UINT8(B_PAWN, pos.board[sq]);
    }
}

/* ── starting position – pieces ──────────────────────────────────────── */
void test_startpos_white_back_rank(void)
{
    TEST_ASSERT_EQUAL_UINT8(W_ROOK, pos.board[A1]);
    TEST_ASSERT_EQUAL_UINT8(W_KNIGHT, pos.board[B1]);
    TEST_ASSERT_EQUAL_UINT8(W_BISHOP, pos.board[C1]);
    TEST_ASSERT_EQUAL_UINT8(W_QUEEN, pos.board[D1]);
    TEST_ASSERT_EQUAL_UINT8(W_KING, pos.board[E1]);
    TEST_ASSERT_EQUAL_UINT8(W_BISHOP, pos.board[F1]);
    TEST_ASSERT_EQUAL_UINT8(W_KNIGHT, pos.board[G1]);
    TEST_ASSERT_EQUAL_UINT8(W_ROOK, pos.board[H1]);
}

void test_startpos_black_back_rank(void)
{
    TEST_ASSERT_EQUAL_UINT8(B_ROOK, pos.board[A8]);
    TEST_ASSERT_EQUAL_UINT8(B_KNIGHT, pos.board[B8]);
    TEST_ASSERT_EQUAL_UINT8(B_BISHOP, pos.board[C8]);
    TEST_ASSERT_EQUAL_UINT8(B_QUEEN, pos.board[D8]);
    TEST_ASSERT_EQUAL_UINT8(B_KING, pos.board[E8]);
    TEST_ASSERT_EQUAL_UINT8(B_BISHOP, pos.board[F8]);
    TEST_ASSERT_EQUAL_UINT8(B_KNIGHT, pos.board[G8]);
    TEST_ASSERT_EQUAL_UINT8(B_ROOK, pos.board[H8]);
}

/* ── emptiness ───────────────────────────────────────────────────────── */
void test_startpos_ranks_3456_are_empty(void)
{
    for (Square sq = A3; sq <= H6; sq++)
    {
        TEST_ASSERT_EQUAL_UINT8(EMPTY, pos.board[sq]);
    }
}

/* ── square count ────────────────────────────────────────────────────── */
void test_square_count_is_64(void)
{
    TEST_ASSERT_EQUAL_INT(64, SQUARE_NB);
}

/* ── bitboard occupancy ──────────────────────────────────────────────── */
void test_occupancy_bitboards_match_startpos(void)
{
    /* occ[WHITE] should have exactly 16 bits (8 pawns + 8 pieces) */
    TEST_ASSERT_EQUAL_INT(16, bit_count(pos.occ[COLOR_IDX(WHITE)]));
    /* occ[BLACK] should have exactly 16 bits */
    TEST_ASSERT_EQUAL_INT(16, bit_count(pos.occ[COLOR_IDX(BLACK)]));
    /* occAll should have 32 bits */
    TEST_ASSERT_EQUAL_INT(32, bit_count(pos.occAll));
    /* occAll = occ[WHITE] | occ[BLACK] */
    TEST_ASSERT_EQUAL_HEX64(
        pos.occ[COLOR_IDX(WHITE)] | pos.occ[COLOR_IDX(BLACK)],
        pos.occAll);
}

void test_piece_bitboards_startpos(void)
{
    /* white pawn bitboard: rank 2 = bits 8-15 */
    TEST_ASSERT_EQUAL_HEX64(0x000000000000FF00ULL, pos.pieces[COLOR_IDX(WHITE)][PAWN]);
    /* black pawn bitboard: rank 7 = bits 48-55 */
    TEST_ASSERT_EQUAL_HEX64(0x00FF000000000000ULL, pos.pieces[COLOR_IDX(BLACK)][PAWN]);
    /* white king at E1 */
    TEST_ASSERT_EQUAL_HEX64(1ULL << E1, pos.pieces[COLOR_IDX(WHITE)][KING]);
    /* black king at E8 */
    TEST_ASSERT_EQUAL_HEX64(1ULL << E8, pos.pieces[COLOR_IDX(BLACK)][KING]);
    /* white knights at B1 and G1 */
    TEST_ASSERT_EQUAL_HEX64((1ULL << B1) | (1ULL << G1), pos.pieces[COLOR_IDX(WHITE)][KNIGHT]);
    /* white bishops at C1 and F1 */
    TEST_ASSERT_EQUAL_HEX64((1ULL << C1) | (1ULL << F1), pos.pieces[COLOR_IDX(WHITE)][BISHOP]);
    /* white rooks at A1 and H1 */
    TEST_ASSERT_EQUAL_HEX64((1ULL << A1) | (1ULL << H1), pos.pieces[COLOR_IDX(WHITE)][ROOK]);
    /* white queen at D1 */
    TEST_ASSERT_EQUAL_HEX64(1ULL << D1, pos.pieces[COLOR_IDX(WHITE)][QUEEN]);
}

void test_kingSq_startpos(void)
{
    TEST_ASSERT_EQUAL_INT(E1, pos.kingSq[COLOR_IDX(WHITE)]);
    TEST_ASSERT_EQUAL_INT(E8, pos.kingSq[COLOR_IDX(BLACK)]);
}

void test_startpos_game_state(void)
{
    TEST_ASSERT_EQUAL_INT(WHITE, pos.sideToMove);
    TEST_ASSERT_EQUAL_UINT8(CASTLE_ALL, pos.castlingRights);
    TEST_ASSERT_EQUAL_INT(SQ_NONE, pos.enPassantSquare);
    TEST_ASSERT_EQUAL_INT(0, pos.fiftyMoveCounter);
    TEST_ASSERT_EQUAL_INT(1, pos.fullmoveNumber);
}

/* ── attack table smoke tests ────────────────────────────────────────── */
void test_knight_attacks_B1(void)
{
    /* Knight on B1 from startpos: can reach A3, C3, D2 */
    uint64_t expected = (1ULL << A3) | (1ULL << C3) | (1ULL << D2);
    TEST_ASSERT_EQUAL_HEX64(expected, knightAttacks[B1]);
}

void test_knight_attacks_G8(void)
{
    /* Knight on G8 from startpos: can reach E7, F6, H6 */
    uint64_t expected = (1ULL << E7) | (1ULL << F6) | (1ULL << H6);
    TEST_ASSERT_EQUAL_HEX64(expected, knightAttacks[G8]);
}

void test_king_attacks_E1(void)
{
    /* King on E1: E2, D1, D2, F1, F2 */
    uint64_t expected = (1ULL << E2) | (1ULL << D1) | (1ULL << D2) | (1ULL << F1) | (1ULL << F2);
    TEST_ASSERT_EQUAL_HEX64(expected, kingAttacks[E1]);
}

void test_pawn_attacks_white_D2(void)
{
    /* white pawn on D2 attacks C3 and E3 */
    uint64_t expected = (1ULL << C3) | (1ULL << E3);
    TEST_ASSERT_EQUAL_HEX64(expected, pawnAttacks[COLOR_IDX(WHITE)][D2]);
}

void test_pawn_attacks_black_D7(void)
{
    /* black pawn on D7 attacks C6 and E6 */
    uint64_t expected = (1ULL << C6) | (1ULL << E6);
    TEST_ASSERT_EQUAL_HEX64(expected, pawnAttacks[COLOR_IDX(BLACK)][D7]);
}

static uint64_t rook_attacks_ref(int sq, uint64_t occ)
{
    uint64_t att = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);

    for (int rr = r + 1; rr <= 7; rr++)
    {
        uint64_t bb = 1ULL << SQUARE(f, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    for (int rr = r - 1; rr >= 0; rr--)
    {
        uint64_t bb = 1ULL << SQUARE(f, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    for (int ff = f + 1; ff <= 7; ff++)
    {
        uint64_t bb = 1ULL << SQUARE(ff, r);
        att |= bb;
        if (occ & bb)
            break;
    }
    for (int ff = f - 1; ff >= 0; ff--)
    {
        uint64_t bb = 1ULL << SQUARE(ff, r);
        att |= bb;
        if (occ & bb)
            break;
    }

    return att;
}

static uint64_t bishop_attacks_ref(int sq, uint64_t occ)
{
    uint64_t att = 0ULL;
    int r = RANK_OF(sq), f = FILE_OF(sq);

    for (int rr = r + 1, ff = f + 1; rr <= 7 && ff <= 7; rr++, ff++)
    {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    for (int rr = r + 1, ff = f - 1; rr <= 7 && ff >= 0; rr++, ff--)
    {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    for (int rr = r - 1, ff = f + 1; rr >= 0 && ff <= 7; rr--, ff++)
    {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb)
            break;
    }
    for (int rr = r - 1, ff = f - 1; rr >= 0 && ff >= 0; rr--, ff--)
    {
        uint64_t bb = 1ULL << SQUARE(ff, rr);
        att |= bb;
        if (occ & bb)
            break;
    }

    return att;
}

/* ── magic bitboard smoke test ───────────────────────────────────────── */
void test_bishop_attacks_empty_board(void)
{
    /* Bishop on C1 on empty board: should see all diagonals */
    uint64_t att = bishopAttacks(C1, 0ULL);
    /* Should include B2, A3, D2, E3, F4, G5, H6 */
    TEST_ASSERT(att & (1ULL << B2));
    TEST_ASSERT(att & (1ULL << A3));
    TEST_ASSERT(att & (1ULL << D2));
    TEST_ASSERT(att & (1ULL << E3));
    /* Should NOT include C1 itself */
    TEST_ASSERT(!(att & (1ULL << C1)));
}

void test_rook_attacks_empty_board(void)
{
    /* Rook on A1 on empty board: should see rank 1 files B-H, file A ranks 2-8 */
    uint64_t att = rookAttacks(A1, 0ULL);
    TEST_ASSERT(att & (1ULL << B1));
    TEST_ASSERT(att & (1ULL << H1));
    TEST_ASSERT(att & (1ULL << A2));
    TEST_ASSERT(att & (1ULL << A8));
    /* Should NOT include A1 itself */
    TEST_ASSERT(!(att & (1ULL << A1)));
}

void test_rook_attacks_blocked(void)
{
    /* Rook on A1 with a fellow piece on A3 and C1 */
    uint64_t occ = (1ULL << A3) | (1ULL << C1);
    uint64_t att = rookAttacks(A1, occ);
    /* Should see A2, B1 (blocked by C1), and not beyond A3/C1 */
    TEST_ASSERT(att & (1ULL << A2));
    TEST_ASSERT(att & (1ULL << B1));
    TEST_ASSERT(att & (1ULL << A3)); /* blocker square is attacked */
    TEST_ASSERT(att & (1ULL << C1)); /* blocker square is attacked */
    /* Should NOT see A4 (blocked by A3) */
    TEST_ASSERT(!(att & (1ULL << A4)));
    /* Should NOT see D1 (blocked by C1) */
    TEST_ASSERT(!(att & (1ULL << D1)));
}

void test_slider_attacks_match_reference_many_occupancies(void)
{
    uint64_t occ_cases[8] = {
        0ULL,
        ~0ULL,
        0xAA55AA55AA55AA55ULL,
        0x55AA55AA55AA55AAULL,
        0x00000000FFFFFFFFULL,
        0xFFFFFFFF00000000ULL,
        0x0F0F0F0F0F0F0F0FULL,
        0xF0F0F0F0F0F0F0F0ULL};

    for (int sq = 0; sq < 64; sq++)
    {
        for (int i = 0; i < 8; i++)
        {
            uint64_t occ = occ_cases[i] & ~(1ULL << sq);
            TEST_ASSERT_EQUAL_HEX64(rook_attacks_ref(sq, occ), rookAttacks(sq, occ));
            TEST_ASSERT_EQUAL_HEX64(bishop_attacks_ref(sq, occ), bishopAttacks(sq, occ));
        }
    }
}

/* ── Move encoding round-trip ────────────────────────────────────────────── */
void test_move_encode_decode(void)
{
    /* quiet move E2->E4 */
    Move m = MOVE_BUILD(E2, E4, 0, MOVE_QUIET);
    TEST_ASSERT_EQUAL_INT(E2, MOVE_FROM(m));
    TEST_ASSERT_EQUAL_INT(E4, MOVE_TO(m));
    TEST_ASSERT_EQUAL_INT(0, MOVE_PROMO(m));
    TEST_ASSERT_EQUAL_INT(MOVE_QUIET, MOVE_FLAG(m));

    /* promotion a7->a8=Q, flag=0 (promo handled via promo field) */
    Move m2 = MOVE_BUILD(A7, A8, 3, 0); /* 3=queen promo */
    TEST_ASSERT_EQUAL_INT(A7, MOVE_FROM(m2));
    TEST_ASSERT_EQUAL_INT(A8, MOVE_TO(m2));
    TEST_ASSERT_EQUAL_INT(3, MOVE_PROMO(m2));
    TEST_ASSERT_EQUAL_INT(0, MOVE_FLAG(m2));

    /* double push */
    Move m3 = MOVE_BUILD(E7, E5, 0, MOVE_DOUBLE_PUSH);
    TEST_ASSERT_EQUAL_INT(E7, MOVE_FROM(m3));
    TEST_ASSERT_EQUAL_INT(E5, MOVE_TO(m3));
    TEST_ASSERT_EQUAL_INT(MOVE_DOUBLE_PUSH, MOVE_FLAG(m3));

    /* castling K-side */
    Move m4 = MOVE_BUILD(E1, G1, 0, MOVE_CASTLE_KS);
    TEST_ASSERT_EQUAL_INT(E1, MOVE_FROM(m4));
    TEST_ASSERT_EQUAL_INT(G1, MOVE_TO(m4));
    TEST_ASSERT_EQUAL_INT(MOVE_CASTLE_KS, MOVE_FLAG(m4));

    /* castling Q-side */
    Move m5 = MOVE_BUILD(E8, C8, 0, MOVE_CASTLE_QS);
    TEST_ASSERT_EQUAL_INT(MOVE_CASTLE_QS, MOVE_FLAG(m5));
}

/* ── pop_lsb smoke test ──────────────────────────────────────────────────── */
void test_pop_lsb(void)
{
    uint64_t bb = (1ULL << A1) | (1ULL << H8) | (1ULL << D4);
    int count = bit_count(bb); /* 3 */
    int squares[3];
    int i = 0;
    while (bb)
    {
        squares[i++] = pop_lsb(&bb);
    }
    TEST_ASSERT_EQUAL_INT(3, count);
    /* squares should be A1, D4, H8 in order (LSB first) */
    TEST_ASSERT_EQUAL_INT(A1, squares[0]);
    TEST_ASSERT_EQUAL_INT(D4, squares[1]);
    TEST_ASSERT_EQUAL_INT(H8, squares[2]);
    /* bb should now be 0 */
    TEST_ASSERT_EQUAL_HEX64(0ULL, bb);
}

/* ── main (Unity runner) ──────────────────────────────────────────────── */
int main(void)
{
    /* one-time bitboard initialisation */
    bitboard_init();

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
    RUN_TEST(test_occupancy_bitboards_match_startpos);
    RUN_TEST(test_piece_bitboards_startpos);
    RUN_TEST(test_kingSq_startpos);
    RUN_TEST(test_startpos_game_state);
    RUN_TEST(test_knight_attacks_B1);
    RUN_TEST(test_knight_attacks_G8);
    RUN_TEST(test_king_attacks_E1);
    RUN_TEST(test_pawn_attacks_white_D2);
    RUN_TEST(test_pawn_attacks_black_D7);
    RUN_TEST(test_bishop_attacks_empty_board);
    RUN_TEST(test_rook_attacks_empty_board);
    RUN_TEST(test_rook_attacks_blocked);
    RUN_TEST(test_slider_attacks_match_reference_many_occupancies);
    RUN_TEST(test_move_encode_decode);
    RUN_TEST(test_pop_lsb);

    return UNITY_END();
}