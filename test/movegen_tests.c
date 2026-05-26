#include "unity.h"
#include "movegen.h"
#include "fen.h"
#include "zobrist.h"
#include <string.h>

static Position pos;

void setUp(void)
{
    position_startpos(&pos);
}

void tearDown(void) { }

/* ── Helper: does moves[] contain a specific move? ───────────────────────── */
static int contains_move(const Move *moves, int n, Move m) {
    for (int i = 0; i < n; i++)
        if (moves[i] == m) return 1;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * apply_move + undo_move roundtrip tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void assert_positions_equal(const Position *saved, const Position *pos)
{
    TEST_ASSERT_EQUAL_INT(saved->sideToMove, pos->sideToMove);
    TEST_ASSERT_EQUAL_INT(saved->castlingRights, pos->castlingRights);
    TEST_ASSERT_EQUAL_INT(saved->enPassantSquare, pos->enPassantSquare);
    TEST_ASSERT_EQUAL_INT(saved->fiftyMoveCounter, pos->fiftyMoveCounter);
    TEST_ASSERT_EQUAL_INT(saved->fullmoveNumber, pos->fullmoveNumber);
    TEST_ASSERT_EQUAL_INT(saved->kingSq[0], pos->kingSq[0]);
    TEST_ASSERT_EQUAL_INT(saved->kingSq[1], pos->kingSq[1]);
    TEST_ASSERT_EQUAL_UINT64(saved->occAll, pos->occAll);
    TEST_ASSERT_EQUAL_UINT64(saved->occ[0], pos->occ[0]);
    TEST_ASSERT_EQUAL_UINT64(saved->occ[1], pos->occ[1]);
    TEST_ASSERT_EQUAL_UINT64(saved->hashKey, pos->hashKey);
    for (int col = 0; col < 2; col++) {
        for (int pt = 0; pt < 7; pt++) {
            TEST_ASSERT_EQUAL_UINT64(saved->pieces[col][pt], pos->pieces[col][pt]);
        }
    }
    for (int sq = 0; sq < 64; sq++) {
        TEST_ASSERT_EQUAL_UINT8(saved->board[sq], pos->board[sq]);
    }
}

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
    Undo u;
    Move mv = MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH);
    apply_move(&pos, mv, &u);

    TEST_ASSERT_EQUAL_INT(E3, pos.enPassantSquare);
    TEST_ASSERT_EQUAL_INT(BLACK, pos.sideToMove);

    undo_move(&pos, &u);
    TEST_ASSERT_EQUAL_INT(WHITE, pos.sideToMove);
    TEST_ASSERT_EQUAL_INT(SQ_NONE, pos.enPassantSquare);
}

/* ── capture resets fifty-move counter ──────────────────────────────── */
void test_capture_resets_fifty(void)
{
    Undo u1, u2, u3;

    /* Move white knight b1-c3, black d7-d5, knight c3xd5 */
    apply_move(&pos, MOVE_BUILD(B1, C3, 0, MOVE_QUIET), &u1);
    apply_move(&pos, MOVE_BUILD(D7, D5, 0, MOVE_DOUBLE_PUSH), &u2);
    pos.fiftyMoveCounter = 7;

    Undo u_cap;
    Move cap = MOVE_BUILD(C3, D5, 0, MOVE_QUIET);
    apply_move(&pos, cap, &u_cap);

    TEST_ASSERT_EQUAL_INT(0, pos.fiftyMoveCounter);

    undo_move(&pos, &u_cap);
    TEST_ASSERT_EQUAL_INT(7, pos.fiftyMoveCounter);

    undo_move(&pos, &u2);
    undo_move(&pos, &u1);
    /* variable u3 not used, remove */
    (void)u3;
}

/* ── pawn push resets fifty-move counter ───────────────────────────── */
void test_pawn_push_resets_fifty(void)
{
    Undo u;
    pos.fiftyMoveCounter = 10;
    Move mv = MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH);
    apply_move(&pos, mv, &u);

    TEST_ASSERT_EQUAL_INT(0, pos.fiftyMoveCounter);

    undo_move(&pos, &u);
    TEST_ASSERT_EQUAL_INT(10, pos.fiftyMoveCounter);
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

    Position saved = pos;

    Undo u;
    Move mv = MOVE_BUILD(E1, E2, 0, MOVE_QUIET);
    apply_move(&pos, mv, &u);

    TEST_ASSERT_EQUAL_INT(0, pos.castlingRights & (CASTLE_WK | CASTLE_WQ));

    undo_move(&pos, &u);
    TEST_ASSERT_EQUAL_INT(saved.castlingRights, pos.castlingRights);
    TEST_ASSERT_NOT_EQUAL(0, saved.castlingRights & CASTLE_WK);
}

/* ── castling rights lost when rook moves ──────────────────────────── */
void test_castling_rights_rook_moves(void)
{
    /* Clear knight so rook on a1 can move */
    pos.board[B1] = EMPTY;
    pos.pieces[0][KNIGHT] &= ~(1ULL << B1);
    pos.occ[0] &= ~(1ULL << B1);
    pos.occAll = pos.occ[0] | pos.occ[1];

    Position saved = pos;

    Undo u;
    Move mv = MOVE_BUILD(A1, A2, 0, MOVE_QUIET);
    apply_move(&pos, mv, &u);

    TEST_ASSERT_EQUAL_INT(0, pos.castlingRights & CASTLE_WQ);
    TEST_ASSERT_NOT_EQUAL(0, pos.castlingRights & CASTLE_WK);

    undo_move(&pos, &u);
    TEST_ASSERT_EQUAL_INT(saved.castlingRights, pos.castlingRights);
}

/* ── en passant capture roundtrip ───────────────────────────────────── */
void test_en_passant_capture_roundtrip(void)
{
    Undo u1, u2, u3;
    /* 1. e2-e4 */
    apply_move(&pos, MOVE_BUILD(E2, E4, 0, MOVE_DOUBLE_PUSH), &u1);
    /* 2. d7-d5 (black) */
    apply_move(&pos, MOVE_BUILD(D7, D5, 0, MOVE_DOUBLE_PUSH), &u2);
    /* 3. e4-d5 en passant */
    Move ep = MOVE_BUILD(E4, D5, 0, MOVE_QUIET);
    apply_move(&pos, ep, &u3);

    /* After ep: white pawn on d5, black pawn on d5 captured */

    undo_move(&pos, &u3);
    /* Verify black pawn is back on d5 */
    TEST_ASSERT_EQUAL_UINT8(B_PAWN, pos.board[D5]);
    /* White pawn back on e4 */
    TEST_ASSERT_EQUAL_UINT8(W_PAWN, pos.board[E4]);

    undo_move(&pos, &u2);
    undo_move(&pos, &u1);
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

    Position saved = pos;

    Undo u;
    Move promo = MOVE_BUILD(A7, A8, 3, MOVE_QUIET); /* promote to queen (promo 3) */
    apply_move(&pos, promo, &u);

    TEST_ASSERT_EQUAL_UINT8(W_QUEEN, pos.board[A8]);
    TEST_ASSERT_EQUAL_UINT8(EMPTY, pos.board[A7]);

    undo_move(&pos, &u);

    TEST_ASSERT_EQUAL_UINT8(W_PAWN, pos.board[A7]);
    TEST_ASSERT_EQUAL_UINT8(EMPTY, pos.board[A8]);
    for (int sq = 0; sq < 64; sq++)
        TEST_ASSERT_EQUAL_UINT8(saved.board[sq], pos.board[sq]);
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

    TEST_ASSERT_EQUAL_UINT8(W_KING, pos.board[E1]);
    TEST_ASSERT_EQUAL_UINT8(W_ROOK, pos.board[H1]);
    TEST_ASSERT_EQUAL_INT(E1, pos.kingSq[0]);
    TEST_ASSERT_EQUAL_INT(saved.castlingRights, pos.castlingRights);
}

/* ── black move increments and decrements fullmove ───────────────────── */
void test_black_move_increments_and_decrements_fullmove(void)
{
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
    TEST_ASSERT_EQUAL_INT(1, pos.fullmoveNumber);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * movegen_pseudo_legal tests
 * ═══════════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════════
 * is_square_attacked tests
 * ═══════════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════════
 * perft tests — known values from Chess Programming Wiki
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── start position ─────────────────────────────────────────────────── */
void test_perft_startpos_depth0(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(1ULL, perft(&pos, 0));
}

void test_perft_startpos_depth1(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(20ULL, perft(&pos, 1));
}

void test_perft_startpos_depth2(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(400ULL, perft(&pos, 2));
}

void test_perft_startpos_depth3(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(8902ULL, perft(&pos, 3));
}

void test_perft_startpos_depth4(void)
{
    position_startpos(&pos);
    TEST_ASSERT_EQUAL_UINT64(197281ULL, perft(&pos, 4));
}

/* ── Kiwipete ──────────────────────────────────────────────────────────
 * r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - */
void test_perft_kiwipete_depth1(void)
{
    /* Use a local position so setUp/tearDown don't interfere */
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", &p));
    TEST_ASSERT_EQUAL_UINT64(48ULL, perft(&p, 1));
}

void test_perft_kiwipete_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", &p));
    TEST_ASSERT_EQUAL_UINT64(2039ULL, perft(&p, 2));
}

void test_perft_kiwipete_depth3(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", &p));
    TEST_ASSERT_EQUAL_UINT64(97862ULL, perft(&p, 3));
}

/* ── Position 3 ────────────────────────────────────────────────────────
 * 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - */
void test_perft_pos3_depth1(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", &p));
    TEST_ASSERT_EQUAL_UINT64(14ULL, perft(&p, 1));
}

void test_perft_pos3_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", &p));
    TEST_ASSERT_EQUAL_UINT64(191ULL, perft(&p, 2));
}

void test_perft_pos3_depth3(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", &p));
    TEST_ASSERT_EQUAL_UINT64(2812ULL, perft(&p, 3));
}

/* ── Position 4 ────────────────────────────────────────────────────────
 * r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1 */
void test_perft_pos4_depth1(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", &p));
    TEST_ASSERT_EQUAL_UINT64(6ULL, perft(&p, 1));
}

void test_perft_pos4_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", &p));
    TEST_ASSERT_EQUAL_UINT64(264ULL, perft(&p, 2));
}

void test_perft_pos4_depth3(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", &p));
    TEST_ASSERT_EQUAL_UINT64(9467ULL, perft(&p, 3));
}

/* ── Position 5 ────────────────────────────────────────────────────────
 * rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8 */
void test_perft_pos5_depth1(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", &p));
    TEST_ASSERT_EQUAL_UINT64(44ULL, perft(&p, 1));
}

void test_perft_pos5_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", &p));
    TEST_ASSERT_EQUAL_UINT64(1486ULL, perft(&p, 2));
}

/* ── Position 6 ────────────────────────────────────────────────────────
 * r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 */
void test_perft_pos6_depth1(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", &p));
    TEST_ASSERT_EQUAL_UINT64(46ULL, perft(&p, 1));
}

void test_perft_pos6_depth2(void)
{
    Position p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", &p));
    TEST_ASSERT_EQUAL_UINT64(2079ULL, perft(&p, 2));
}

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


/* ── main (Unity runner) ──────────────────────────────────────────────── */
int main(void)
{
    bitboard_init();

    UNITY_BEGIN();

    RUN_TEST(test_apply_undo_knight_roundtrip);
    RUN_TEST(test_apply_double_push_sets_ep);
    RUN_TEST(test_capture_resets_fifty);
    RUN_TEST(test_pawn_push_resets_fifty);
    RUN_TEST(test_castling_rights_king_move);
    RUN_TEST(test_castling_rights_rook_moves);
    RUN_TEST(test_en_passant_capture_roundtrip);
    RUN_TEST(test_promotion_undo);
    RUN_TEST(test_castling_make_unmake);
    RUN_TEST(test_black_move_increments_and_decrements_fullmove);

    RUN_TEST(test_apply_undo_all_moves_in_standard_positions);
    RUN_TEST(test_castling_rights_lost_by_rook_capture);
    RUN_TEST(test_all_promotion_types_roundtrip);
    RUN_TEST(test_en_passant_restores_previous_ep_square);

    RUN_TEST(test_startpos_white_move_count);
    RUN_TEST(test_startpos_includes_e2e4);
    RUN_TEST(test_startpos_includes_b1c3);
    RUN_TEST(test_startpos_does_not_include_blocked_castling);
    RUN_TEST(test_castling_generated_when_path_clear);
    RUN_TEST(test_black_move_count_after_white_move);
    RUN_TEST(test_en_passant_generated);
    RUN_TEST(test_no_en_passant_without_ep_square);
    RUN_TEST(test_promotions_for_white_pawn_on_a7);
    RUN_TEST(test_no_moves_leave_own_king_in_check);

    RUN_TEST(test_e4_attacked_by_black_knight_on_f6);
    RUN_TEST(test_d2_attacked_by_white_queen_on_d1);
    RUN_TEST(test_f7_attacked_by_white_bishop_on_c4);
    RUN_TEST(test_g2_attacked_by_black_pawn_on_h3);

    RUN_TEST(test_perft_startpos_depth0);
    RUN_TEST(test_perft_startpos_depth1);
    RUN_TEST(test_perft_startpos_depth2);
    RUN_TEST(test_perft_startpos_depth3);
    RUN_TEST(test_perft_startpos_depth4);
    RUN_TEST(test_perft_kiwipete_depth1);
    RUN_TEST(test_perft_kiwipete_depth2);
    RUN_TEST(test_perft_kiwipete_depth3);
    RUN_TEST(test_perft_pos3_depth1);
    RUN_TEST(test_perft_pos3_depth2);
    RUN_TEST(test_perft_pos3_depth3);
    RUN_TEST(test_perft_pos4_depth1);
    RUN_TEST(test_perft_pos4_depth2);
    RUN_TEST(test_perft_pos4_depth3);
    RUN_TEST(test_perft_pos5_depth1);
    RUN_TEST(test_perft_pos5_depth2);
    RUN_TEST(test_perft_pos6_depth1);
    RUN_TEST(test_perft_pos6_depth2);

    RUN_TEST(test_zobrist_hash_keys_startpos);
    RUN_TEST(test_zobrist_hash_keys_kiwipete);
    RUN_TEST(test_zobrist_hash_keys_pos4);

    return UNITY_END();
}