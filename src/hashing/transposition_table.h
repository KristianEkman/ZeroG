#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include <stddef.h>
#include <stdint.h>

#include "boards.h"

typedef enum {
    TT_BOUND_NONE = 0,
    TT_BOUND_EXACT = 1,
    TT_BOUND_LOWER = 2,
    TT_BOUND_UPPER = 3
} TranspositionBound;

typedef struct {
    uint64_t key;
    Move best_move;
    int score;
    unsigned depth;
    unsigned generation;
    TranspositionBound bound;
} TranspositionEntry;

typedef struct {
    void *storage;
    size_t bucket_count;
    unsigned generation;
} TranspositionTable;

int transposition_table_init(TranspositionTable *table, size_t size_bytes);
void transposition_table_free(TranspositionTable *table);
void transposition_table_clear(TranspositionTable *table);
void transposition_table_next_generation(TranspositionTable *table);
int transposition_table_store(TranspositionTable *table,
                              uint64_t key,
                              unsigned depth,
                              unsigned ply,
                              int score,
                              TranspositionBound bound,
                              Move best_move);
int transposition_table_lookup(const TranspositionTable *table,
                               uint64_t key,
                               unsigned ply,
                               TranspositionEntry *entry);

#endif