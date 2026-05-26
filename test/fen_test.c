#include "unity.h"
#include "boards.h"
#include "fen.h"
#include <string.h>

/* ── Unity boilerplate ───────────────────────────────────────────────────── */
void setUp(void) { }
void tearDown(void) { }

/* ── helper: check that two positions are bitwise-equal ──────────────────── */
static void assert_positions_equal(const Position *a, const Position *b)
{
    TEST_ASSERT_EQUAL_INT(a->sideToMove, b->sideToMove);
    TEST_ASSERT_EQUAL_UINT8(a->castlingRights, b->castlingRights);
    TEST_ASSERT_EQUAL_INT(a->enPassantSquare, b->enPassantSquare);
    TEST_ASSERT_EQUAL_INT(a->fiftyMoveCounter, b->fiftyMoveCounter);
    TEST_ASSERT_EQUAL_INT(a->fullmoveNumber, b->fullmoveNumber);
    TEST_ASSERT_EQUAL_INT(a->kingSq[COLOR_IDX(WHITE)], b->kingSq[COLOR_IDX(WHITE)]);
    TEST_ASSERT_EQUAL_INT(a->kingSq[COLOR_IDX(BLACK)], b->kingSq[COLOR_IDX(BLACK)]);
    TEST_ASSERT_EQUAL_HEX64(a->occAll, b->occAll);
    TEST_ASSERT_EQUAL_HEX64(a->occ[COLOR_IDX(WHITE)], b->occ[COLOR_IDX(WHITE)]);
    TEST_ASSERT_EQUAL_HEX64(a->occ[COLOR_IDX(BLACK)], b->occ[COLOR_IDX(BLACK)]);
    for (int pt = PAWN; pt <= KING; pt++) {
        TEST_ASSERT_EQUAL_HEX64(a->pieces[COLOR_IDX(WHITE)][pt],
                                b->pieces[COLOR_IDX(WHITE)][pt]);
        TEST_ASSERT_EQUAL_HEX64(a->pieces[COLOR_IDX(BLACK)][pt],
                                b->pieces[COLOR_IDX(BLACK)][pt]);
    }
    for (int sq = 0; sq < 64; sq++) {
        TEST_ASSERT_EQUAL_UINT8(a->board[sq], b->board[sq]);
    }
}

/* ── FEN parse of standard startpos ──────────────────────────────────────── */
void test_fen_parse_startpos(void)
{
    Position ref, pos;
    position_startpos(&ref);

    int rc = fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &pos);
    TEST_ASSERT_EQUAL_INT(0, rc);
    assert_positions_equal(&ref, &pos);
}

/* ── FEN parse with black to move, no castling, en passant ──────────────── */
void test_fen_parse_advanced(void)
{
    Position pos;
    int rc = fen_parse("4k3/8/8/8/8/8/8/4K3 b - - 0 1", &pos);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_EQUAL_INT(BLACK, pos.sideToMove);
    TEST_ASSERT_EQUAL_UINT8(0, pos.castlingRights);
    TEST_ASSERT_EQUAL_INT(SQ_NONE, pos.enPassantSquare);
    TEST_ASSERT_EQUAL_INT(0, pos.fiftyMoveCounter);
    TEST_ASSERT_EQUAL_INT(1, pos.fullmoveNumber);

    /* white king on E1 */
    TEST_ASSERT_EQUAL_UINT8(W_KING, pos.board[E1]);
    TEST_ASSERT_EQUAL_INT(E1, pos.kingSq[COLOR_IDX(WHITE)]);

    /* black king on E8 */
    TEST_ASSERT_EQUAL_UINT8(B_KING, pos.board[E8]);
    TEST_ASSERT_EQUAL_INT(E8, pos.kingSq[COLOR_IDX(BLACK)]);
}

/* ── FEN parse with en passant square ────────────────────────────────────── */
void test_fen_parse_en_passant(void)
{
    Position pos;
    int rc = fen_parse("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", &pos);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_EQUAL_INT(BLACK, pos.sideToMove);
    TEST_ASSERT_EQUAL_INT(E3, pos.enPassantSquare);
    TEST_ASSERT_EQUAL_UINT8(W_PAWN, pos.board[E4]);
    TEST_ASSERT_EQUAL_UINT8(EMPTY, pos.board[E2]);
}

/* ── FEN parse with larger clocks ────────────────────────────────────────── */
void test_fen_parse_clocks(void)
{
    Position pos;
    int rc = fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 13 42", &pos);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(13, pos.fiftyMoveCounter);
    TEST_ASSERT_EQUAL_INT(42, pos.fullmoveNumber);
}

/* ── FEN parse errors ────────────────────────────────────────────────────── */
void test_fen_parse_bad_null(void)
{
    TEST_ASSERT_EQUAL_INT(-1, fen_parse(NULL, NULL));
    Position pos;
    TEST_ASSERT_EQUAL_INT(-1, fen_parse(NULL, &pos));
}

void test_fen_parse_truncated(void)
{
    Position pos;
    /* missing fullmove number */
    TEST_ASSERT_EQUAL_INT(-1, fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0", &pos));
}

void test_fen_parse_bad_side(void)
{
    Position pos;
    TEST_ASSERT_EQUAL_INT(-1, fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1", &pos));
}

void test_fen_parse_bad_castling_char(void)
{
    Position pos;
    TEST_ASSERT_EQUAL_INT(-1, fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w XQkq - 0 1", &pos));
}

void test_fen_parse_illegal_piece_char(void)
{
    Position pos;
    TEST_ASSERT_EQUAL_INT(-1, fen_parse("rnbqkXnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &pos));
}

void test_fen_parse_row_too_short(void)
{
    Position pos;
    TEST_ASSERT_EQUAL_INT(-1, fen_parse("rnbqkbn/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &pos));
}

void test_fen_parse_row_too_long(void)
{
    Position pos;
    TEST_ASSERT_EQUAL_INT(-1, fen_parse("rnbqkbnr1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &pos));
}

/* ── FEN serialize startpos ──────────────────────────────────────────────── */
void test_fen_serialize_startpos(void)
{
    Position pos;
    position_startpos(&pos);

    char buf[128];
    int len = fen_serialize(&pos, buf);
    TEST_ASSERT(len > 0);
    TEST_ASSERT(len < (int)sizeof(buf));

    /* Parse back and verify round-trip */
    Position pos2;
    int rc = fen_parse(buf, &pos2);
    TEST_ASSERT_EQUAL_INT(0, rc);
    assert_positions_equal(&pos, &pos2);
}

/* ── FEN serialize no castling rights ────────────────────────────────────── */
void test_fen_serialize_no_castling(void)
{
    Position pos;
    position_startpos(&pos);
    pos.castlingRights = 0;

    char buf[128];
    fen_serialize(&pos, buf);
    /* castling field must be "-" */
    TEST_ASSERT(strstr(buf, " - ") != NULL);
}

/* ── FEN serialize with en passant ───────────────────────────────────────── */
void test_fen_serialize_en_passant(void)
{
    Position pos;
    position_startpos(&pos);
    pos.sideToMove = BLACK;
    pos.enPassantSquare = D3;

    char buf[128];
    fen_serialize(&pos, buf);

    /* Parse back */
    Position pos2;
    fen_parse(buf, &pos2);
    TEST_ASSERT_EQUAL_INT(D3, pos2.enPassantSquare);
    TEST_ASSERT_EQUAL_INT(BLACK, pos2.sideToMove);
}

/* ── FEN serialize NULL guard ────────────────────────────────────────────── */
void test_fen_serialize_null(void)
{
    TEST_ASSERT_EQUAL_INT(-1, fen_serialize(NULL, NULL));
    char buf[10];
    TEST_ASSERT_EQUAL_INT(-1, fen_serialize(NULL, buf));
    Position pos;
    TEST_ASSERT_EQUAL_INT(-1, fen_serialize(&pos, NULL));
}

/* ── FEN round-trip: complex position ────────────────────────────────────── */
void test_fen_roundtrip_complex(void)
{
    const char *original =
        "r2qk2r/pp2bppp/2p1bn2/3p4/3P4/2N1BN2/PP2PPPP/R2QKB1R w KQkq d6 5 12";
    Position pos;
    int rc = fen_parse(original, &pos);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char buf[128];
    int len = fen_serialize(&pos, buf);
    TEST_ASSERT(len > 0);

    Position pos2;
    rc = fen_parse(buf, &pos2);
    TEST_ASSERT_EQUAL_INT(0, rc);

    assert_positions_equal(&pos, &pos2);
}

/* ── FEN round-trip: empty en passant and halfmove=0 ────────────────────── */
void test_fen_roundtrip_no_ep_no_castle(void)
{
    const char *original =
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQ - 4 6";
    Position pos;
    TEST_ASSERT_EQUAL_INT(0, fen_parse(original, &pos));

    char buf[128];
    fen_serialize(&pos, buf);

    Position pos2;
    TEST_ASSERT_EQUAL_INT(0, fen_parse(buf, &pos2));
    assert_positions_equal(&pos, &pos2);
}

/* ── FEN serialize produces NUL-terminated string ────────────────────────── */
void test_fen_serialize_nul_terminated(void)
{
    Position pos;
    position_startpos(&pos);
    char buf[128];
    int len = fen_serialize(&pos, buf);
    TEST_ASSERT_EQUAL_INT(len, (int)strlen(buf));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[len]);
}

/* ── FEN normalization: castling rights ordering and duplicates ──────────── */
void test_fen_normalize_castling_rights(void)
{
    const char *out_of_order =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w QKkq - 0 1";
    Position pos;
    TEST_ASSERT_EQUAL_INT(0, fen_parse(out_of_order, &pos));

    char buf[128];
    fen_serialize(&pos, buf);
    TEST_ASSERT_EQUAL_STRING("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", buf);

    const char *duplicates =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w QKqkQ - 0 1";
    TEST_ASSERT_EQUAL_INT(0, fen_parse(duplicates, &pos));
    fen_serialize(&pos, buf);
    TEST_ASSERT_EQUAL_STRING("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", buf);

    const char *subset_out_of_order =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w qQ - 0 1";
    TEST_ASSERT_EQUAL_INT(0, fen_parse(subset_out_of_order, &pos));
    fen_serialize(&pos, buf);
    TEST_ASSERT_EQUAL_STRING("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w Qq - 0 1", buf);
}

/* ── FEN normalization: missing fields (4-field FEN) ─────────────────────── */
void test_fen_normalize_missing_clocks(void)
{
    const char *four_fields =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
    Position pos;
    TEST_ASSERT_EQUAL_INT(0, fen_parse(four_fields, &pos));
    TEST_ASSERT_EQUAL_INT(0, pos.fiftyMoveCounter);
    TEST_ASSERT_EQUAL_INT(1, pos.fullmoveNumber);

    char buf[128];
    fen_serialize(&pos, buf);
    TEST_ASSERT_EQUAL_STRING("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", buf);
}

/* ── FEN normalization: fullmove number zero/negative ───────────────────── */
void test_fen_normalize_invalid_fullmove(void)
{
    const char *zero_fullmove =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0";
    Position pos;
    TEST_ASSERT_EQUAL_INT(0, fen_parse(zero_fullmove, &pos));
    TEST_ASSERT_EQUAL_INT(1, pos.fullmoveNumber);

    char buf[128];
    fen_serialize(&pos, buf);
    TEST_ASSERT_EQUAL_STRING("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", buf);
}

/* ── FEN normalization: consecutive digit empty squares ──────────────────── */
void test_fen_normalize_consecutive_empty_digits(void)
{
    const char *consecutive_digits =
        "rnbqkbnr/pppppppp/314/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Position pos;
    TEST_ASSERT_EQUAL_INT(0, fen_parse(consecutive_digits, &pos));

    char buf[128];
    fen_serialize(&pos, buf);
    TEST_ASSERT_EQUAL_STRING("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", buf);
}

/* ── FEN parse check: other side to move king in check ────────────────── */
void test_fen_parse_other_king_in_check(void)
{
    Position pos;

    /* 1. White to move, Black king in check by White Rook on e1 (illegal state) */
    int rc1 = fen_parse("4k3/8/8/8/8/8/8/4R3 w - - 0 1", &pos);
    TEST_ASSERT_EQUAL_INT(-1, rc1);

    /* 2. Black to move, Black king in check by White Rook on e1 (legal state) */
    int rc2 = fen_parse("4k3/8/8/8/8/8/8/4R3 b - - 0 1", &pos);
    TEST_ASSERT_EQUAL_INT(0, rc2);
}


/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    bitboard_init();

    UNITY_BEGIN();

    RUN_TEST(test_fen_parse_startpos);
    RUN_TEST(test_fen_parse_advanced);
    RUN_TEST(test_fen_parse_en_passant);
    RUN_TEST(test_fen_parse_clocks);
    RUN_TEST(test_fen_parse_bad_null);
    RUN_TEST(test_fen_parse_truncated);
    RUN_TEST(test_fen_parse_bad_side);
    RUN_TEST(test_fen_parse_bad_castling_char);
    RUN_TEST(test_fen_parse_illegal_piece_char);
    RUN_TEST(test_fen_parse_row_too_short);
    RUN_TEST(test_fen_parse_row_too_long);
    RUN_TEST(test_fen_serialize_startpos);
    RUN_TEST(test_fen_serialize_no_castling);
    RUN_TEST(test_fen_serialize_en_passant);
    RUN_TEST(test_fen_serialize_null);
    RUN_TEST(test_fen_roundtrip_complex);
    RUN_TEST(test_fen_roundtrip_no_ep_no_castle);
    RUN_TEST(test_fen_serialize_nul_terminated);
    RUN_TEST(test_fen_normalize_castling_rights);
    RUN_TEST(test_fen_normalize_missing_clocks);
    RUN_TEST(test_fen_normalize_invalid_fullmove);
    RUN_TEST(test_fen_normalize_consecutive_empty_digits);
    RUN_TEST(test_fen_parse_other_king_in_check);

    return UNITY_END();
}