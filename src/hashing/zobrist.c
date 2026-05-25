#include "zobrist.h"

#include <stddef.h>

enum {
    ZOBRIST_PIECE_CAPACITY = 16,
    ZOBRIST_CASTLING_COMBINATIONS = 16
};

static uint64_t zobrist_piece_square_keys[ZOBRIST_PIECE_CAPACITY][SQUARE_NB];
static uint64_t zobrist_castling_keys[ZOBRIST_CASTLING_COMBINATIONS];
static uint64_t zobrist_en_passant_keys[SQUARE_NB];
static uint64_t zobrist_black_to_move_key;
static int zobrist_initialized;

static uint64_t zobrist_splitmix64(uint64_t *state)
{
    uint64_t value = (*state += UINT64_C(0x9e3779b97f4a7c15));

    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

static void zobrist_ensure_initialized(void)
{
    uint64_t state = UINT64_C(0x6a09e667f3bcc909);

    if (zobrist_initialized) {
        return;
    }

    for (int piece = 0; piece < ZOBRIST_PIECE_CAPACITY; ++piece) {
        for (int square = 0; square < SQUARE_NB; ++square) {
            zobrist_piece_square_keys[piece][square] = zobrist_splitmix64(&state);
        }
    }

    for (int rights = 0; rights < ZOBRIST_CASTLING_COMBINATIONS; ++rights) {
        zobrist_castling_keys[rights] = zobrist_splitmix64(&state);
    }

    for (int square = 0; square < SQUARE_NB; ++square) {
        zobrist_en_passant_keys[square] = zobrist_splitmix64(&state);
    }

    zobrist_black_to_move_key = zobrist_splitmix64(&state);
    zobrist_initialized = 1;
}

uint64_t zobrist_compute_key(const Position *board)
{
    uint64_t key = 0;

    if (board == NULL) {
        return 0;
    }

    zobrist_ensure_initialized();

    for (int square = 0; square < SQUARE_NB; ++square) {
        Piece piece = board->board[square];

        if (piece != EMPTY) {
            key ^= zobrist_piece_square_keys[piece][square];
        }
    }

    key ^= zobrist_castling_keys[board->castlingRights & 0x0f];

    if (board->enPassantSquare >= A1 && board->enPassantSquare < SQUARE_NB) {
        key ^= zobrist_en_passant_keys[board->enPassantSquare];
    }

    if (board->sideToMove == BLACK) {
        key ^= zobrist_black_to_move_key;
    }

    return key;
}

uint64_t zobrist_piece_key(Piece piece, Square square)
{
    if (piece == EMPTY || piece >= ZOBRIST_PIECE_CAPACITY ||
        square < A1 || square >= SQUARE_NB) {
        return 0;
    }

    zobrist_ensure_initialized();
    return zobrist_piece_square_keys[piece][square];
}

uint64_t zobrist_castling_key(unsigned castling_rights)
{
    zobrist_ensure_initialized();
    return zobrist_castling_keys[castling_rights & 0x0f];
}

uint64_t zobrist_en_passant_key(Square square)
{
    if (square < A1 || square >= SQUARE_NB) {
        return 0;
    }

    zobrist_ensure_initialized();
    return zobrist_en_passant_keys[square];
}

uint64_t zobrist_side_to_move_key(void)
{
    zobrist_ensure_initialized();
    return zobrist_black_to_move_key;
}