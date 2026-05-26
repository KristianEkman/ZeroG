#include "test_helpers.h"

/* ── Legal Movegen King Safety Tests ──────────────────────────────────── */

void test_legal_movegen_ep_discovered_check(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    // White King on a5, White Pawn on e5, Black Pawn on d5 (ep square d6), Black Rook on h5
    int res = fen_parse("8/8/8/K2pP2r/8/8/8/4k3 w - d6 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);

    Move legal[MAX_MOVES];
    int n = movegen_legal(&p, legal);

    // e5xd6 ep is MOVE_BUILD(E5, D6, 0, MOVE_QUIET)
    Move ep_move = MOVE_BUILD(E5, D6, 0, MOVE_QUIET);
    TEST_ASSERT_FALSE(contains_move(legal, n, ep_move));
}

void test_legal_movegen_castling_restrictions(void)
{
    Position p;
    Move legal[MAX_MOVES];
    int n;

    // 1. Castling illegal when in check
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/8/8/8/1b6/8/8/R3K2R w KQkq - 0 1", &p));
    n = movegen_legal(&p, legal);
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(E1, G1, 0, MOVE_CASTLE_KS)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(E1, C1, 0, MOVE_CASTLE_QS)));

    // 2. Castling illegal when passing through check (f1 attacked)
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/5r2/8/8/8/8/8/R3K2R w KQkq - 0 1", &p));
    n = movegen_legal(&p, legal);
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(E1, G1, 0, MOVE_CASTLE_KS)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(E1, C1, 0, MOVE_CASTLE_QS))); // d1 is not attacked

    // 3. Castling illegal when landing on check (g1 attacked)
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/8/8/8/3b4/8/8/R3K2R w KQkq - 0 1", &p));
    n = movegen_legal(&p, legal);
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(E1, G1, 0, MOVE_CASTLE_KS)));

    // 4. Rook passing through check is legal (b1 attacked by black rook on b8)
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("1r2k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", &p));
    n = movegen_legal(&p, legal);
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(E1, C1, 0, MOVE_CASTLE_QS)));
}

void test_legal_movegen_double_check_evasions(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    // White King on e1, Black Knight on f3, Black Queen on e2 (double check)
    int res = fen_parse("r3k2r/8/8/8/8/5n2/4q3/4K3 w kq - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);

    Move legal[MAX_MOVES];
    int n = movegen_legal(&p, legal);

    // Only King moves are legal. Any non-king move should be illegal.
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_INT(KING, PIECE_TYPE(p.board[MOVE_FROM(legal[i])]));
    }
}

void test_legal_movegen_absolute_pins(void)
{
    Position p;
    Move legal[MAX_MOVES];
    int n;

    // 1. Pinned White Bishop on d2 by Black Bishop on a5
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/b7/8/8/3B4/4K3 w - - 0 1", &p));
    n = movegen_legal(&p, legal);
    
    // Pinned bishop can only move along a5-e1 diagonal (c3, b4, a5)
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(D2, C3, 0, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(D2, B4, 0, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(D2, A5, 0, MOVE_QUIET)));
    
    // Other moves are illegal
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(D2, E3, 0, MOVE_QUIET)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(D2, F4, 0, MOVE_QUIET)));

    // 2. Pinned White Rook on e2 by Black Queen on e5
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("4k3/8/8/4q3/8/8/4R3/4K3 w - - 0 1", &p));
    n = movegen_legal(&p, legal);
    
    // Rook can only move along e-file (e3, e4, e5)
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(E2, E3, 0, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(E2, E4, 0, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(E2, E5, 0, MOVE_QUIET)));
    
    // Rook moving to other files is illegal
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(E2, D2, 0, MOVE_QUIET)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(E2, F2, 0, MOVE_QUIET)));
}

static void verify_legal_moves_safety_recursive(Position *p, int depth)
{
    Move moves[MAX_MOVES];
    int n = movegen_legal(p, moves);

    for (int i = 0; i < n; i++) {
        Undo u;
        Color us = p->sideToMove;
        apply_move(p, moves[i], &u);

        int king_sq = p->kingSq[COLOR_IDX(us)];
        TEST_ASSERT_FALSE(is_square_attacked(p, king_sq, p->sideToMove));

        if (depth > 1) {
            verify_legal_moves_safety_recursive(p, depth - 1);
        }

        undo_move(p, &u);
    }
}

void test_legal_movegen_safety_recursive_startpos(void)
{
    Position p;
    position_startpos(&p);
    verify_legal_moves_safety_recursive(&p, 3);
}

void test_legal_movegen_safety_recursive_kiwipete(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", &p));
    verify_legal_moves_safety_recursive(&p, 2);
}

void test_legal_movegen_safety_recursive_pos3(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", &p));
    verify_legal_moves_safety_recursive(&p, 2);
}

void test_legal_movegen_safety_recursive_pos4(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", &p));
    verify_legal_moves_safety_recursive(&p, 2);
}

void test_legal_movegen_safety_recursive_pos5(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", &p));
    verify_legal_moves_safety_recursive(&p, 2);
}

void test_legal_movegen_safety_recursive_pos6(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", &p));
    verify_legal_moves_safety_recursive(&p, 2);
}

void test_promotion_captures_all_combinations(void)
{
    Position p;
    int res;
    Undo u;
    Move promo;

    PieceType target_pieces[] = { KNIGHT, BISHOP, ROOK, QUEEN };

    /* ── White Promotion Captures ── */
    for (int cap_idx = 0; cap_idx < 4; cap_idx++) {
        PieceType cap_pt = target_pieces[cap_idx];
        for (int promo_type = 0; promo_type < 4; promo_type++) {
            char fen[100];
            char target_char = (cap_pt == KNIGHT) ? 'n' :
                               (cap_pt == BISHOP) ? 'b' :
                               (cap_pt == ROOK)   ? 'r' : 'q';
            sprintf(fen, "1%c2k3/P7/8/8/8/8/8/4K3 w - - 0 1", target_char);
            
            memset(&p, 0, sizeof(p));
            res = fen_parse(fen, &p);
            TEST_ASSERT_EQUAL_INT(0, res);

            Position saved = p;
            promo = MOVE_BUILD(A7, B8, promo_type, MOVE_QUIET);

            p.fiftyMoveCounter = 10;
            saved.fiftyMoveCounter = 10;

            apply_move(&p, promo, &u);

            PieceType expected_pt = (promo_type == 0) ? KNIGHT :
                                    (promo_type == 1) ? BISHOP :
                                    (promo_type == 2) ? ROOK : QUEEN;
            
            TEST_ASSERT_EQUAL_INT(BLACK, p.sideToMove);
            TEST_ASSERT_EQUAL_INT(0, p.fiftyMoveCounter);
            TEST_ASSERT_EQUAL_UINT8(MAKE_PIECE(WHITE, expected_pt), p.board[B8]);
            TEST_ASSERT_EQUAL_UINT8(EMPTY, p.board[A7]);
            TEST_ASSERT_EQUAL_UINT8(MAKE_PIECE(BLACK, cap_pt), u.captured);
            TEST_ASSERT_EQUAL_INT(B8, u.cap_sq);

            TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

            undo_move(&p, &u);
            assert_positions_equal(&saved, &p);
        }
    }

    /* ── Black Promotion Captures ── */
    for (int cap_idx = 0; cap_idx < 4; cap_idx++) {
        PieceType cap_pt = target_pieces[cap_idx];
        for (int promo_type = 0; promo_type < 4; promo_type++) {
            char fen[100];
            char target_char = (cap_pt == KNIGHT) ? 'N' :
                               (cap_pt == BISHOP) ? 'B' :
                               (cap_pt == ROOK)   ? 'R' : 'Q';
            sprintf(fen, "4K3/8/8/8/8/8/p7/1%c2k3 b - - 0 1", target_char);

            memset(&p, 0, sizeof(p));
            res = fen_parse(fen, &p);
            TEST_ASSERT_EQUAL_INT(0, res);

            Position saved = p;
            promo = MOVE_BUILD(A2, B1, promo_type, MOVE_QUIET);

            p.fiftyMoveCounter = 10;
            saved.fiftyMoveCounter = 10;

            apply_move(&p, promo, &u);

            PieceType expected_pt = (promo_type == 0) ? KNIGHT :
                                    (promo_type == 1) ? BISHOP :
                                    (promo_type == 2) ? ROOK : QUEEN;
            
            TEST_ASSERT_EQUAL_INT(WHITE, p.sideToMove);
            TEST_ASSERT_EQUAL_INT(0, p.fiftyMoveCounter);
            TEST_ASSERT_EQUAL_UINT8(MAKE_PIECE(BLACK, expected_pt), p.board[B1]);
            TEST_ASSERT_EQUAL_UINT8(EMPTY, p.board[A2]);
            TEST_ASSERT_EQUAL_UINT8(MAKE_PIECE(WHITE, cap_pt), u.captured);
            TEST_ASSERT_EQUAL_INT(B1, u.cap_sq);

            TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

            undo_move(&p, &u);
            assert_positions_equal(&saved, &p);
        }
    }
}

void test_promotion_captures_castling_rights(void)
{
    Position p;
    int res;
    Undo u;
    Move promo;

    /* 1. White captures Black Rook on H8, promoting to Queen */
    memset(&p, 0, sizeof(p));
    res = fen_parse("4k2r/6P1/8/8/8/8/8/4K3 w k - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(CASTLE_BK, p.castlingRights);
    Position saved1 = p;
    promo = MOVE_BUILD(G7, H8, 3, MOVE_QUIET);
    apply_move(&p, promo, &u);
    TEST_ASSERT_EQUAL_INT(0, p.castlingRights);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    undo_move(&p, &u);
    assert_positions_equal(&saved1, &p);

    /* 2. White captures Black Rook on A8, promoting to Queen */
    memset(&p, 0, sizeof(p));
    res = fen_parse("r3k3/1P6/8/8/8/8/8/4K3 w q - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(CASTLE_BQ, p.castlingRights);
    Position saved2 = p;
    promo = MOVE_BUILD(B7, A8, 3, MOVE_QUIET);
    apply_move(&p, promo, &u);
    TEST_ASSERT_EQUAL_INT(0, p.castlingRights);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    undo_move(&p, &u);
    assert_positions_equal(&saved2, &p);

    /* 3. Black captures White Rook on H1, promoting to Queen */
    memset(&p, 0, sizeof(p));
    res = fen_parse("4K3/8/8/8/8/8/6p1/4k2R b K - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(CASTLE_WK, p.castlingRights);
    Position saved3 = p;
    promo = MOVE_BUILD(G2, H1, 3, MOVE_QUIET);
    apply_move(&p, promo, &u);
    TEST_ASSERT_EQUAL_INT(0, p.castlingRights);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    undo_move(&p, &u);
    assert_positions_equal(&saved3, &p);

    /* 4. Black captures White Rook on A1, promoting to Queen */
    memset(&p, 0, sizeof(p));
    res = fen_parse("4K3/8/8/8/8/8/1p6/R3k3 b Q - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(CASTLE_WQ, p.castlingRights);
    Position saved4 = p;
    promo = MOVE_BUILD(B2, A1, 3, MOVE_QUIET);
    apply_move(&p, promo, &u);
    TEST_ASSERT_EQUAL_INT(0, p.castlingRights);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    undo_move(&p, &u);
    assert_positions_equal(&saved4, &p);
}

void test_promotion_captures_movegen_pinned(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("5b1r/6PP/7K/8/8/8/8/4k3 w - - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);

    Move legal[MAX_MOVES];
    int n = movegen_legal(&p, legal);

    /* Capturing on f8 should be legal for all 4 promotion types */
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(G7, F8, 0, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(G7, F8, 1, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(G7, F8, 2, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(G7, F8, 3, MOVE_QUIET)));

    /* Capturing on h8 should be illegal for all 4 promotion types */
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(G7, H8, 0, MOVE_QUIET)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(G7, H8, 1, MOVE_QUIET)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(G7, H8, 2, MOVE_QUIET)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(G7, H8, 3, MOVE_QUIET)));
}

void test_promotion_captures_movegen_resolves_check(void)
{
    Position p;
    Move legal[MAX_MOVES];
    int n;

    /* 1. Single check: Rook on e8 checks White King on e1. White Pawn on d7 can capture e8. */
    memset(&p, 0, sizeof(p));
    int res = fen_parse("4r3/3P4/8/8/8/8/8/4K3 w - - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);

    n = movegen_legal(&p, legal);

    /* Pushing to d8 is illegal because it doesn't resolve the check */
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(D7, D8, 0, MOVE_QUIET)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(D7, D8, 3, MOVE_QUIET)));

    /* Capturing on e8 is legal */
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(D7, E8, 0, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(D7, E8, 1, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(D7, E8, 2, MOVE_QUIET)));
    TEST_ASSERT_TRUE(contains_move(legal, n, MOVE_BUILD(D7, E8, 3, MOVE_QUIET)));

    /* 2. Double check: Rook on e8 and Knight on c2 check King on e1.
     * White Pawn on d7 capturing e8 is now illegal because of Knight check. */
    memset(&p, 0, sizeof(p));
    res = fen_parse("4r3/3P4/8/8/8/8/2n5/4K3 w - - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);

    n = movegen_legal(&p, legal);

    /* Capturing on e8 must be illegal */
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(D7, E8, 0, MOVE_QUIET)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(D7, E8, 1, MOVE_QUIET)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(D7, E8, 2, MOVE_QUIET)));
    TEST_ASSERT_FALSE(contains_move(legal, n, MOVE_BUILD(D7, E8, 3, MOVE_QUIET)));

    /* All legal moves must be King moves */
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_INT(KING, PIECE_TYPE(p.board[MOVE_FROM(legal[i])]));
    }
}
