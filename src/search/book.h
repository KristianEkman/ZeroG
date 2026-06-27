#ifndef BOOK_H
#define BOOK_H

#include "boards.h"
#include <stdint.h>

extern int use_book;
extern char book_path[512];

/* Computes the standard Polyglot Zobrist hash of the given position */
uint64_t book_polyglot_hash(const Position *board);

/* Probes the opening book specified in book_path for the current position.
 * Returns the matched legal internal Move, or 0 if no book move is found. */
Move book_probe(const Position *board);

#endif /* BOOK_H */
