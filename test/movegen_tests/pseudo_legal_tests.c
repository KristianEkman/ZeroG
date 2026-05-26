#include "test_helpers.h"

void test_startpos_white_move_count(void)
{
    Move moves[MAX_MOVES];
    int n = movegen_pseudo_legal(&pos, moves);
    TEST_ASSERT_EQUAL_INT(20, n);
}

void test_startpos_includes_e2e4(void)
{
    Move moves[MAX_MOVES];
    int n = movegen_pseudo_legal(&pos, moves);
    Move e2e4 = MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH);
    TEST_ASSERT(contains_move(moves, n, e2e4));
}

void test_startpos_includes_b1c3(void)
{
    Move moves[MAX_MOVES];
    int n = movegen_pseudo_legal(&pos, moves);
    Move b1c3 = MOVE_BUILD(B1, C3, 0, MOVE_QUIET);
    TEST_ASSERT(contains_move(moves, n, b1c3));
}

void test_startpos_does_not_include_blocked_castling(void)
{
    Move moves[MAX_MOVES];
    int n = movegen_pseudo_legal(&pos, moves);
    Move castles = MOVE_BUILD(E1, G1, 0, MOVE_CASTLE_KS);
    TEST_ASSERT_FALSE(contains_move(moves, n, castles));
}

void test_castling_generated_when_path_clear(void)
{
    pos.board[F1] = EMPTY;
    pos.board[G1] = EMPTY;
    pos.pieces[0][BISHOP] &= ~(1ULL << F1);
    pos.pieces[0][KNIGHT] &= ~(1ULL << G1);
    pos.occ[0] &= ~((1ULL << F1) | (1ULL << G1));
    pos.occAll = pos.occ[0] | pos.occ[1];

    Move moves[MAX_MOVES];
    int n = movegen_pseudo_legal(&pos, moves);
    Move castles = MOVE_BUILD(E1, G1, 0, MOVE_CASTLE_KS);
    TEST_ASSERT(contains_move(moves, n, castles));
}

void test_black_move_count_after_white_move(void)
{
    Undo u;
    apply_move(&pos, MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH), &u);

    Move moves[MAX_MOVES];
    int n = movegen_pseudo_legal(&pos, moves);
    TEST_ASSERT_EQUAL_INT(20, n);
    undo_move(&pos, &u);
}

void test_en_passant_generated(void)
{
    Undo u1, u2;
    apply_move(&pos, MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH), &u1);
    apply_move(&pos, MOVE_BUILD(D7, D5, 0, MOVE_DOUBLE_PUSH), &u2);

    Move moves[MAX_MOVES];
    int n = movegen_pseudo_legal(&pos, moves);
    Move ep = MOVE_BUILD(E4, D5, 0, MOVE_QUIET);
    TEST_ASSERT(contains_move(moves, n, ep));

    undo_move(&pos, &u2);
    undo_move(&pos, &u1);
}

void test_no_en_passant_without_ep_square(void)
{
    Move moves[MAX_MOVES];
    int n = movegen_pseudo_legal(&pos, moves);
    Move ep = MOVE_BUILD(E4, D5, 0, MOVE_QUIET);
    TEST_ASSERT_FALSE(contains_move(moves, n, ep));
}

void test_promotions_for_white_pawn_on_a7(void)
{
    position_startpos(&pos);
    pos.board[A8] = EMPTY;
    pos.pieces[1][ROOK] &= ~(1ULL << A8);
    pos.occ[1] &= ~(1ULL << A8);
    pos.board[A7] = W_PAWN;
    pos.pieces[0][PAWN] |= (1ULL << A7);
    pos.occ[0] |= (1ULL << A7);
    pos.board[A2] = EMPTY;
    pos.pieces[0][PAWN] &= ~(1ULL << A2);
    pos.occ[0] &= ~(1ULL << A2);
    pos.occAll = pos.occ[0] | pos.occ[1];
    pos.sideToMove = WHITE;

    Move moves[MAX_MOVES];
    int n = movegen_pseudo_legal(&pos, moves);
    int promo_count = 0;
    for (int i = 0; i < n; i++) {
        if (MOVE_FROM(moves[i]) == A7 && MOVE_TO(moves[i]) == A8)
            promo_count++;
    }
    TEST_ASSERT_EQUAL_INT(4, promo_count);
}

void test_no_moves_leave_own_king_in_check(void)
{
    /* Set up: white king on e1, black queen on e2 (checking). */
    position_startpos(&pos);
    pos.board[E2] = B_QUEEN;
    pos.pieces[1][QUEEN] |= (1ULL << E2);
    pos.occ[1] |= (1ULL << E2);
    pos.pieces[0][PAWN] &= ~(1ULL << E2);
    pos.occ[0] &= ~(1ULL << E2);
    pos.occAll = pos.occ[0] | pos.occ[1];
    pos.sideToMove = WHITE;

    Move legal[MAX_MOVES];
    int n = movegen_legal(&pos, legal);

    for (int i = 0; i < n; i++) {
        Undo u;
        apply_move(&pos, legal[i], &u);
        int ks = pos.kingSq[0];
        TEST_ASSERT_FALSE(is_square_attacked(&pos, ks, BLACK));
        undo_move(&pos, &u);
    }
}
