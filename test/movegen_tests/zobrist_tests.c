#include "test_helpers.h"

/* ── Zobrist hash key validation tests ────────────────────────────────── */
static void verify_hash_keys_recursive(Position *pos, int depth)
{
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(pos), pos->hashKey);

    if (depth == 0) {
        return;
    }

    Move moves[MAX_MOVES];
    int n = movegen_legal(pos, moves);

    for (int i = 0; i < n; i++) {
        Undo u;
        uint64_t expected_old_hash = pos->hashKey;
        apply_move(pos, moves[i], &u);
        TEST_ASSERT_EQUAL_UINT64(expected_old_hash, u.old_hash);
        TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(pos), pos->hashKey);

        verify_hash_keys_recursive(pos, depth - 1);

        undo_move(pos, &u);
        TEST_ASSERT_EQUAL_UINT64(expected_old_hash, pos->hashKey);
        TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(pos), pos->hashKey);
    }
}

void test_zobrist_hash_keys_startpos(void)
{
    position_startpos(&pos);
    verify_hash_keys_recursive(&pos, 3);
}

void test_zobrist_hash_keys_kiwipete(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", &p));
    verify_hash_keys_recursive(&p, 2);
}

void test_zobrist_hash_keys_pos4(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", &p));
    verify_hash_keys_recursive(&p, 2);
}

void test_zobrist_castling_white_ks(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("4k3/8/8/8/8/8/8/4K2R w K - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(E1, G1, 0, MOVE_CASTLE_KS);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    TEST_ASSERT_EQUAL_INT(0, p.castlingRights);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_castling_white_qs(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("4k3/8/8/8/8/8/8/R3K3 w Q - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(E1, C1, 0, MOVE_CASTLE_QS);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    TEST_ASSERT_EQUAL_INT(0, p.castlingRights);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_castling_black_ks(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("4k2r/8/8/8/8/8/8/4K3 b k - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(E8, G8, 0, MOVE_CASTLE_KS);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    TEST_ASSERT_EQUAL_INT(0, p.castlingRights);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_castling_black_qs(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("r3k3/8/8/8/8/8/8/4K3 b q - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(E8, C8, 0, MOVE_CASTLE_QS);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    TEST_ASSERT_EQUAL_INT(0, p.castlingRights);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_en_passant_capture_white(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("8/8/8/3pP3/8/8/8/4K3 w - d6 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(D6, p.enPassantSquare);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(E5, D6, 0, MOVE_QUIET);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    TEST_ASSERT_EQUAL_INT(SQ_NONE, p.enPassantSquare);
    TEST_ASSERT_EQUAL_UINT8(EMPTY, p.board[D5]);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_en_passant_capture_black(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("4K3/8/8/8/3Pp3/8/8/8 b - d3 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(D3, p.enPassantSquare);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(E4, D3, 0, MOVE_QUIET);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);
    TEST_ASSERT_EQUAL_INT(SQ_NONE, p.enPassantSquare);
    TEST_ASSERT_EQUAL_UINT8(EMPTY, p.board[D4]);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_en_passant_ignored_cleared(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("8/8/8/3pP3/8/8/8/4K3 w - d6 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(D6, p.enPassantSquare);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(E1, D1, 0, MOVE_QUIET);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_INT(SQ_NONE, p.enPassantSquare);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_promotions_and_promo_captures(void)
{
    Position p;
    int res;
    Undo u;
    Move promo;

    for (int capture = 0; capture <= 1; capture++) {
        for (int promo_type = 0; promo_type < 4; promo_type++) {
            memset(&p, 0, sizeof(p));
            if (capture) {
                res = fen_parse("1r6/P7/8/8/8/8/8/4K3 w - - 0 1", &p);
            } else {
                res = fen_parse("8/P7/8/8/8/8/8/4K3 w - - 0 1", &p);
            }
            TEST_ASSERT_EQUAL_INT(0, res);
            TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

            Position saved = p;
            int to_sq = capture ? B8 : A8;
            promo = MOVE_BUILD(A7, to_sq, promo_type, MOVE_QUIET);

            apply_move(&p, promo, &u);
            TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

            undo_move(&p, &u);
            assert_positions_equal(&saved, &p);
        }
    }
}

void test_zobrist_castling_rights_loss_king_move(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(E1, E2, 0, MOVE_QUIET);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_INT(CASTLE_BK | CASTLE_BQ, p.castlingRights);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_castling_rights_loss_rook_move(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(A1, A2, 0, MOVE_QUIET);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_INT(CASTLE_WK | CASTLE_BK | CASTLE_BQ, p.castlingRights);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_castling_rights_loss_rook_captured(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    int res = fen_parse("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", &p);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    Position saved = p;
    Undo u;
    Move mv = MOVE_BUILD(A8, A1, 0, MOVE_QUIET);
    apply_move(&p, mv, &u);
    TEST_ASSERT_EQUAL_INT(CASTLE_WK | CASTLE_BK, p.castlingRights);
    TEST_ASSERT_EQUAL_UINT64(zobrist_compute_key(&p), p.hashKey);

    undo_move(&p, &u);
    assert_positions_equal(&saved, &p);
}

void test_zobrist_hash_keys_pos3(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", &p));
    verify_hash_keys_recursive(&p, 3);
}

void test_zobrist_hash_keys_pos5(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", &p));
    verify_hash_keys_recursive(&p, 2);
}

void test_zobrist_hash_keys_pos6(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", &p));
    verify_hash_keys_recursive(&p, 2);
}
