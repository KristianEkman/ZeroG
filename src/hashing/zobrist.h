#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <stdint.h>

#include "board.h"

uint64_t zobrist_compute_key(const Board *board);
uint64_t zobrist_piece_key(Piece piece, Square square);
uint64_t zobrist_castling_key(unsigned castling_rights);
uint64_t zobrist_en_passant_key(Square square);
uint64_t zobrist_side_to_move_key(void);

#endif