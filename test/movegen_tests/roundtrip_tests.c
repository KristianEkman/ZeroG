#include "test_helpers.h"

/* ── quiet knight move ───────────────────────────────────────────────── */
void test_apply_undo_knight_roundtrip(void)
{
    Position saved = pos;
    Undo u;

    Move mv = MOVE_BUILD(B1, C3, 0, MOVE_QUIET);
    apply_move(&pos, mv, &u);
    TEST_ASSERT_EQUAL_INT(BLACK, pos.sideToMove);

    undo_move(&pos, &u);

    assert_positions_equal(&saved, &pos);
}

/* ── e2-e4 double push sets en passant ──────────────────────────────── */
void test_apply_double_push_sets_ep(void)
{
    Position saved = pos;
    Undo u;
    Move mv = MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH);
    apply_move(&pos, mv, &u);

    TEST_ASSERT_EQUAL_INT(E3, pos.enPassantSquare);
    TEST_ASSERT_EQUAL_INT(BLACK, pos.sideToMove);

    undo_move(&pos, &u);
    assert_positions_equal(&saved, &pos);
}

/* ── capture resets fifty-move counter ──────────────────────────────── */
void test_capture_resets_fifty(void)
{
    Position saved = pos;
    Undo u1, u2, u3;

    /* Move white knight b1-c3, black d7-d5, knight c3xd5 */
    apply_move(&pos, MOVE_BUILD(B1, C3, 0, MOVE_QUIET), &u1);
    apply_move(&pos, MOVE_BUILD(D7, D5, 0, MOVE_DOUBLE_PUSH), &u2);
    pos.fiftyMoveCounter = 7;
    pos.hashKey = zobrist_compute_key(&pos);

    Position saved_before_cap = pos;

    Undo u_cap;
    Move cap = MOVE_BUILD(C3, D5, 0, MOVE_QUIET);
    apply_move(&pos, cap, &u_cap);

    TEST_ASSERT_EQUAL_INT(0, pos.fiftyMoveCounter);

    undo_move(&pos, &u_cap);
    assert_positions_equal(&saved_before_cap, &pos);

    undo_move(&pos, &u2);
    undo_move(&pos, &u1);
    assert_positions_equal(&saved, &pos);
    (void)u3;
}

/* ── pawn push resets fifty-move counter ───────────────────────────── */
void test_pawn_push_resets_fifty(void)
{
    pos.fiftyMoveCounter = 10;
    pos.hashKey = zobrist_compute_key(&pos);
    Position saved = pos;

    Undo u;
    Move mv = MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH);
    apply_move(&pos, mv, &u);

    TEST_ASSERT_EQUAL_INT(0, pos.fiftyMoveCounter);

    undo_move(&pos, &u);
    assert_positions_equal(&saved, &pos);
}

/* ── castling rights are lost when king moves ──────────────────────── */
void test_castling_rights_king_move(void)
{
    /* Clear knight so king can move. Do it on the Position directly
     * (before any apply_move so sideToMove is still WHITE). */
    pos.board[G1] = EMPTY;
    pos.pieces[0][KNIGHT] &= ~(1ULL << G1);
    pos.occ[0] &= ~(1ULL << G1);
    pos.occAll = pos.occ[0] | pos.occ[1];
    pos.hashKey = zobrist_compute_key(&pos);

    Position saved = pos;

    Undo u;
    Move mv = MOVE_BUILD(E1, E2, 0, MOVE_QUIET);
    apply_move(&pos, mv, &u);

    TEST_ASSERT_EQUAL_INT(0, pos.castlingRights & (CASTLE_WK | CASTLE_WQ));

    undo_move(&pos, &u);
    assert_positions_equal(&saved, &pos);
}

/* ── castling rights lost when rook moves ──────────────────────────── */
void test_castling_rights_rook_moves(void)
{
    /* Clear knight so rook on a1 can move */
    pos.board[B1] = EMPTY;
    pos.pieces[0][KNIGHT] &= ~(1ULL << B1);
    pos.occ[0] &= ~(1ULL << B1);
    pos.occAll = pos.occ[0] | pos.occ[1];
    pos.hashKey = zobrist_compute_key(&pos);

    Position saved = pos;

    Undo u;
    Move mv = MOVE_BUILD(A1, A2, 0, MOVE_QUIET);
    apply_move(&pos, mv, &u);

    TEST_ASSERT_EQUAL_INT(0, pos.castlingRights & CASTLE_WQ);
    TEST_ASSERT_NOT_EQUAL(0, pos.castlingRights & CASTLE_WK);

    undo_move(&pos, &u);
    assert_positions_equal(&saved, &pos);
}

/* ── en passant capture roundtrip ───────────────────────────────────── */
void test_en_passant_capture_roundtrip(void)
{
    Position saved = pos;
    Undo u1, u2, u3;
    /* 1. e2-e4 */
    apply_move(&pos, MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH), &u1);
    /* 2. d7-d5 (black) */
    apply_move(&pos, MOVE_BUILD(D7, D5, 0, MOVE_DOUBLE_PUSH), &u2);
    
    Position saved_before_ep = pos;
    /* 3. e4-d5 en passant */
    Move ep = MOVE_BUILD(E4, D5, 0, MOVE_QUIET);
    apply_move(&pos, ep, &u3);

    /* After ep: white pawn on d5, black pawn on d5 captured */

    undo_move(&pos, &u3);
    assert_positions_equal(&saved_before_ep, &pos);

    undo_move(&pos, &u2);
    undo_move(&pos, &u1);
    assert_positions_equal(&saved, &pos);
}

/* ── promotion undo ─────────────────────────────────────────────────── */
void test_promotion_undo(void)
{
    /* Set up white pawn on a7, black rook removed from a8 */
    position_startpos(&pos);
    pos.board[A8] = EMPTY;
    pos.pieces[1][ROOK] &= ~(1ULL << A8);
    pos.occ[1] &= ~(1ULL << A8);

    pos.board[A7] = W_PAWN;
    pos.pieces[0][PAWN] |= (1ULL << A7);
    pos.occ[0] |= (1ULL << A7);
    /* Remove pawn from a2 so no duplicate */
    pos.board[A2] = EMPTY;
    pos.pieces[0][PAWN] &= ~(1ULL << A2);
    pos.occ[0] &= ~(1ULL << A2);
    pos.occAll = pos.occ[0] | pos.occ[1];
    pos.sideToMove = WHITE;
    pos.hashKey = zobrist_compute_key(&pos);

    Position saved = pos;

    Undo u;
    Move promo = MOVE_BUILD(A7, A8, 3, MOVE_QUIET); /* promote to queen (promo 3) */
    apply_move(&pos, promo, &u);

    TEST_ASSERT_EQUAL_UINT8(W_QUEEN, pos.board[A8]);
    TEST_ASSERT_EQUAL_UINT8(EMPTY, pos.board[A7]);

    undo_move(&pos, &u);
    assert_positions_equal(&saved, &pos);
}

/* ── castling make/unmake ───────────────────────────────────────────── */
void test_castling_make_unmake(void)
{
    /* Clear f1,g1 for white king-side castling */
    pos.board[F1] = EMPTY;
    pos.board[G1] = EMPTY;
    pos.pieces[0][BISHOP] &= ~(1ULL << F1);
    pos.pieces[0][KNIGHT] &= ~(1ULL << G1);
    pos.occ[0] &= ~((1ULL << F1) | (1ULL << G1));
    pos.occAll = pos.occ[0] | pos.occ[1];
    pos.hashKey = zobrist_compute_key(&pos);

    Position saved = pos;

    Undo u;
    Move cast = MOVE_BUILD(E1, G1, 0, MOVE_CASTLE_KS);
    apply_move(&pos, cast, &u);

    TEST_ASSERT_EQUAL_UINT8(W_KING, pos.board[G1]);
    TEST_ASSERT_EQUAL_UINT8(EMPTY, pos.board[E1]);
    TEST_ASSERT_EQUAL_UINT8(W_ROOK, pos.board[F1]);
    TEST_ASSERT_EQUAL_UINT8(EMPTY, pos.board[H1]);
    TEST_ASSERT_EQUAL_INT(G1, pos.kingSq[0]);

    undo_move(&pos, &u);
    assert_positions_equal(&saved, &pos);
}

/* ── black move increments and decrements fullmove ───────────────────── */
void test_black_move_increments_and_decrements_fullmove(void)
{
    Position saved = pos;
    Undo u1, u2;

    /* 1. White moves (fullmoveNumber stays 1) */
    apply_move(&pos, MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH), &u1);
    TEST_ASSERT_EQUAL_INT(1, pos.fullmoveNumber);

    /* 2. Black moves (fullmoveNumber increments to 2) */
    apply_move(&pos, MOVE_BUILD(E7, E5, 0, MOVE_DOUBLE_PUSH), &u2);
    TEST_ASSERT_EQUAL_INT(2, pos.fullmoveNumber);

    /* 3. Undo Black move (fullmoveNumber decrements to 1) */
    undo_move(&pos, &u2);
    TEST_ASSERT_EQUAL_INT(1, pos.fullmoveNumber);

    /* 4. Undo White move (fullmoveNumber stays 1) */
    undo_move(&pos, &u1);
    assert_positions_equal(&saved, &pos);
}

void test_apply_undo_all_moves_in_standard_positions(void)
{
    const char *fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"
    };
    int num_fens = sizeof(fens) / sizeof(fens[0]);

    for (int f = 0; f < num_fens; f++) {
        Position original;
        memset(&original, 0, sizeof(original));
        int res = fen_parse(fens[f], &original);
        TEST_ASSERT_EQUAL_INT(0, res);

        Move moves[MAX_MOVES];
        int n = movegen_pseudo_legal(&original, moves);
        TEST_ASSERT(n > 0);

        for (int i = 0; i < n; i++) {
            Position current = original;
            Undo u;

            apply_move(&current, moves[i], &u);

            /* Assert sideToMove flipped */
            TEST_ASSERT_EQUAL_INT(OPPOSITE(original.sideToMove), current.sideToMove);

            /* Assert position has changed (hashKey should differ) */
            TEST_ASSERT_NOT_EQUAL(original.hashKey, current.hashKey);

            undo_move(&current, &u);

            /* Verify exact restoration */
            assert_positions_equal(&original, &current);
        }
    }
}

void test_castling_rights_lost_by_rook_capture(void)
{
    Position p;
    int res;
    Undo u;
    Move cap;

    /* 1. White captures Black Rook on H8 */
    memset(&p, 0, sizeof(p));
    res = fen_parse("r3k2r/6B1/8/8/8/8/8/R3K2R w KQkq - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    Position saved1 = p;
    cap = MOVE_BUILD(G7, H8, 0, MOVE_QUIET);
    apply_move(&p, cap, &u);
    TEST_ASSERT_EQUAL_INT(CASTLE_WK | CASTLE_WQ | CASTLE_BQ, p.castlingRights);
    undo_move(&p, &u);
    assert_positions_equal(&saved1, &p);

    /* 2. White captures Black Rook on A8 */
    memset(&p, 0, sizeof(p));
    res = fen_parse("r3k2r/1B6/8/8/8/8/8/R3K2R w KQkq - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    Position saved2 = p;
    cap = MOVE_BUILD(B7, A8, 0, MOVE_QUIET);
    apply_move(&p, cap, &u);
    TEST_ASSERT_EQUAL_INT(CASTLE_WK | CASTLE_WQ | CASTLE_BK, p.castlingRights);
    undo_move(&p, &u);
    assert_positions_equal(&saved2, &p);

    /* 3. Black captures White Rook on H1 */
    memset(&p, 0, sizeof(p));
    res = fen_parse("r3k2r/8/8/8/8/8/6b1/R3K2R b KQkq - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    Position saved3 = p;
    cap = MOVE_BUILD(G2, H1, 0, MOVE_QUIET);
    apply_move(&p, cap, &u);
    TEST_ASSERT_EQUAL_INT(CASTLE_WQ | CASTLE_BK | CASTLE_BQ, p.castlingRights);
    undo_move(&p, &u);
    assert_positions_equal(&saved3, &p);

    /* 4. Black captures White Rook on A1 */
    memset(&p, 0, sizeof(p));
    res = fen_parse("r3k2r/8/8/8/8/8/1b6/R3K2R b KQkq - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    Position saved4 = p;
    cap = MOVE_BUILD(B2, A1, 0, MOVE_QUIET);
    apply_move(&p, cap, &u);
    TEST_ASSERT_EQUAL_INT(CASTLE_WK | CASTLE_BK | CASTLE_BQ, p.castlingRights);
    undo_move(&p, &u);
    assert_positions_equal(&saved4, &p);
}

void test_all_promotion_types_roundtrip(void)
{
    Position p;
    int res;
    Undo u;
    Move promo;

    /* ── White Promotions ── */
    for (int capture = 0; capture <= 1; capture++) {
        for (int promo_type = 0; promo_type < 4; promo_type++) {
            memset(&p, 0, sizeof(p));
            res = fen_parse("1r6/P7/8/8/8/8/8/4K3 w - - 0 1", &p);
            TEST_ASSERT_EQUAL_INT(0, res);

            Position saved = p;
            int to_sq = capture ? B8 : A8;
            promo = MOVE_BUILD(A7, to_sq, promo_type, MOVE_QUIET);

            apply_move(&p, promo, &u);

            PieceType expected_pt = (promo_type == 0) ? KNIGHT :
                                    (promo_type == 1) ? BISHOP :
                                    (promo_type == 2) ? ROOK : QUEEN;
            TEST_ASSERT_EQUAL_UINT8(MAKE_PIECE(WHITE, expected_pt), p.board[to_sq]);
            TEST_ASSERT_EQUAL_UINT8(EMPTY, p.board[A7]);

            undo_move(&p, &u);
            assert_positions_equal(&saved, &p);
        }
    }

    /* ── Black Promotions ── */
    for (int capture = 0; capture <= 1; capture++) {
        for (int promo_type = 0; promo_type < 4; promo_type++) {
            memset(&p, 0, sizeof(p));
            res = fen_parse("4K3/8/8/8/8/8/p7/1R6 b - - 0 1", &p);
            TEST_ASSERT_EQUAL_INT(0, res);

            Position saved = p;
            int to_sq = capture ? B1 : A1;
            promo = MOVE_BUILD(A2, to_sq, promo_type, MOVE_QUIET);

            apply_move(&p, promo, &u);

            PieceType expected_pt = (promo_type == 0) ? KNIGHT :
                                    (promo_type == 1) ? BISHOP :
                                    (promo_type == 2) ? ROOK : QUEEN;
            TEST_ASSERT_EQUAL_UINT8(MAKE_PIECE(BLACK, expected_pt), p.board[to_sq]);
            TEST_ASSERT_EQUAL_UINT8(EMPTY, p.board[A2]);

            undo_move(&p, &u);
            assert_positions_equal(&saved, &p);
        }
    }
}

void test_en_passant_restores_previous_ep_square(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(D6, p.enPassantSquare);

    Position saved1 = p;
    Undo u1;

    /* Move White Knight b1-c3 (quiet move, clears ep square d6) */
    Move quiet = MOVE_BUILD(B1, C3, 0, MOVE_QUIET);
    apply_move(&p, quiet, &u1);
    TEST_ASSERT_EQUAL_INT(SQ_NONE, p.enPassantSquare);

    undo_move(&p, &u1);
    TEST_ASSERT_EQUAL_INT(D6, p.enPassantSquare);
    assert_positions_equal(&saved1, &p);

    /* Move White Pawn g2-g4 (double push, changes ep square from d6 to g3) */
    Position saved2 = p;
    Undo u2;
    Move dp = MOVE_BUILD(G2, G4, 0, MOVE_DOUBLE_PUSH);
    apply_move(&p, dp, &u2);
    TEST_ASSERT_EQUAL_INT(G3, p.enPassantSquare);

    undo_move(&p, &u2);
    TEST_ASSERT_EQUAL_INT(D6, p.enPassantSquare);
    assert_positions_equal(&saved2, &p);
}
