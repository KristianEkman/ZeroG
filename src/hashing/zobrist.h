#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <stdint.h>

#include "boards.h"

void zobrist_init(void);

extern uint64_t zobrist_piece_square_keys[16][64];
extern uint64_t zobrist_castling_keys[16];
extern uint64_t zobrist_en_passant_keys[64];
extern uint64_t zobrist_black_to_move_key;

uint64_t zobrist_compute_key(const Position *board);

static inline uint64_t zobrist_piece_key(Piece piece, Square square)
{
    return zobrist_piece_square_keys[piece][square];
}

static inline uint64_t zobrist_castling_key(unsigned castling_rights)
{
    return zobrist_castling_keys[castling_rights & 0x0f];
}

static inline uint64_t zobrist_en_passant_key(Square square)
{
    return zobrist_en_passant_keys[square];
}

static inline uint64_t zobrist_side_to_move_key(void)
{
    return zobrist_black_to_move_key;
}

#endif