#include "unity.h"
#include "boards.h"
#include "uci.h"
#include "fen.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// Capture UCI output helper
static void run_uci_command(UciState *state, const char *command, char *out_buf, size_t buf_size, int *should_quit)
{
    FILE *tmp = tmpfile();
    TEST_ASSERT_NOT_NULL(tmp);

    int rc = uci_handle_line(state, command, tmp, should_quit);
    TEST_ASSERT_EQUAL_INT(0, rc);

    rewind(tmp);
    size_t n = fread(out_buf, 1, buf_size - 1, tmp);
    out_buf[n] = '\0';
    fclose(tmp);
}

void test_uci_handshake(void)
{
    UciState state;
    TEST_ASSERT_EQUAL_INT(0, uci_state_init(&state));

    char buf[2048];
    int should_quit = 0;
    run_uci_command(&state, "uci", buf, sizeof(buf), &should_quit);

    TEST_ASSERT_NOT_NULL(strstr(buf, "id name ChessAI2027"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "id author"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "uciok"));
    TEST_ASSERT_EQUAL_INT(0, should_quit);
}

void test_uci_isready(void)
{
    UciState state;
    TEST_ASSERT_EQUAL_INT(0, uci_state_init(&state));

    char buf[1024];
    int should_quit = 0;
    run_uci_command(&state, "isready", buf, sizeof(buf), &should_quit);

    TEST_ASSERT_NOT_NULL(strstr(buf, "readyok"));
    TEST_ASSERT_EQUAL_INT(0, should_quit);
}

void test_uci_ucinewgame(void)
{
    UciState state;
    TEST_ASSERT_EQUAL_INT(0, uci_state_init(&state));

    char buf[1024];
    int should_quit = 0;
    run_uci_command(&state, "ucinewgame", buf, sizeof(buf), &should_quit);

    // ucinewgame outputs nothing directly but initializes board
    TEST_ASSERT_EQUAL_INT(0, should_quit);
    TEST_ASSERT_EQUAL_UINT8(W_KING, state.board.board[E1]);
}

void test_uci_position_startpos(void)
{
    UciState state;
    TEST_ASSERT_EQUAL_INT(0, uci_state_init(&state));

    char buf[1024];
    int should_quit = 0;
    
    // Apply position command
    run_uci_command(&state, "position startpos", buf, sizeof(buf), &should_quit);
    TEST_ASSERT_EQUAL_UINT8(W_KING, state.board.board[E1]);

    // Apply startpos with moves
    run_uci_command(&state, "position startpos moves e2e4 e7e5", buf, sizeof(buf), &should_quit);
    TEST_ASSERT_EQUAL_UINT8(W_PAWN, state.board.board[E4]);
    TEST_ASSERT_EQUAL_UINT8(B_PAWN, state.board.board[E5]);
}

void test_uci_position_fen(void)
{
    UciState state;
    TEST_ASSERT_EQUAL_INT(0, uci_state_init(&state));

    char buf[1024];
    int should_quit = 0;
    
    // Check FEN with empty/spaces and specific position
    run_uci_command(&state, "position fen rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", buf, sizeof(buf), &should_quit);
    TEST_ASSERT_EQUAL_INT(BLACK, state.board.sideToMove);
    TEST_ASSERT_EQUAL_UINT8(W_PAWN, state.board.board[E4]);
    TEST_ASSERT_EQUAL_INT(E3, state.board.enPassantSquare);
}

void test_uci_setoption(void)
{
    UciState state;
    TEST_ASSERT_EQUAL_INT(0, uci_state_init(&state));

    char buf[1024];
    int should_quit = 0;

    // Test different options
    run_uci_command(&state, "setoption name Hash value 32", buf, sizeof(buf), &should_quit);
    run_uci_command(&state, "setoption name Threads value 2", buf, sizeof(buf), &should_quit);
    run_uci_command(&state, "setoption name LMR_Base value 5", buf, sizeof(buf), &should_quit);
    run_uci_command(&state, "setoption name SaveQuietPositionsFile value temp_quiet.epd", buf, sizeof(buf), &should_quit);
    run_uci_command(&state, "setoption name UseNN value false", buf, sizeof(buf), &should_quit);

    // Verify option error handling
    FILE *tmp = tmpfile();
    int rc = uci_handle_line(&state, "setoption name NonExistentOption value 123", tmp, &should_quit);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    fclose(tmp);

    // Reset option so it doesn't affect other tests
    run_uci_command(&state, "setoption name SaveQuietPositionsFile value <empty>", buf, sizeof(buf), &should_quit);
}

void test_uci_go(void)
{
    UciState state;
    TEST_ASSERT_EQUAL_INT(0, uci_state_init(&state));

    char buf[1024];
    int should_quit = 0;

    // First set starting position
    run_uci_command(&state, "position startpos", buf, sizeof(buf), &should_quit);

    // Run simple search
    run_uci_command(&state, "go depth 1", buf, sizeof(buf), &should_quit);
    TEST_ASSERT_NOT_NULL(strstr(buf, "bestmove"));

    // Run movetime search
    run_uci_command(&state, "go movetime 10", buf, sizeof(buf), &should_quit);
    TEST_ASSERT_NOT_NULL(strstr(buf, "bestmove"));

    // Clean up any quiet positions output file created during search
    remove("temp_quiet.epd");
}

void test_uci_quit(void)
{
    UciState state;
    TEST_ASSERT_EQUAL_INT(0, uci_state_init(&state));

    char buf[1024];
    int should_quit = 0;
    run_uci_command(&state, "quit", buf, sizeof(buf), &should_quit);

    TEST_ASSERT_EQUAL_INT(1, should_quit);
}

void test_uci_misc(void)
{
    UciState state;
    TEST_ASSERT_EQUAL_INT(0, uci_state_init(&state));
    
    char buf[1024];
    int should_quit = 0;
    
    // Check empty command line (should return 0, no output)
    run_uci_command(&state, "", buf, sizeof(buf), &should_quit);
    TEST_ASSERT_EQUAL_STRING("", buf);
    
    // Check spaces only
    run_uci_command(&state, "   ", buf, sizeof(buf), &should_quit);
    TEST_ASSERT_EQUAL_STRING("", buf);

    // Check invalid command
    FILE *tmp = tmpfile();
    int rc = uci_handle_line(&state, "invalid_command_name", tmp, &should_quit);
    TEST_ASSERT_EQUAL_INT(-1, rc); // Unrecognized commands return -1 in uci_handle_line
    fclose(tmp);
}

int main(void)
{
    bitboard_init();
    UNITY_BEGIN();
    RUN_TEST(test_uci_handshake);
    RUN_TEST(test_uci_isready);
    RUN_TEST(test_uci_ucinewgame);
    RUN_TEST(test_uci_position_startpos);
    RUN_TEST(test_uci_position_fen);
    RUN_TEST(test_uci_setoption);
    RUN_TEST(test_uci_go);
    RUN_TEST(test_uci_quit);
    RUN_TEST(test_uci_misc);
    return UNITY_END();
}
