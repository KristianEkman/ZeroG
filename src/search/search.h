#ifndef SEARCH_H
#define SEARCH_H

#include <stdio.h>
#include "boards.h"

typedef struct {
    unsigned depth;
    unsigned soft_time_limit_ms;
    unsigned hard_time_limit_ms;
} SearchLimits;

typedef struct {
    Move best_move;
    int score;
    int has_legal_move;
    uint64_t node_count;
} SearchResult;

FILE *search_set_log_output(FILE *output);
int search_best_move_with_limits(const Position *board, const SearchLimits *limits, SearchResult *result);
void search_reset_stop_request(void);
void search_request_stop(void);
int search_compute_time_limits(const Position *board,
                               unsigned depth,
                               unsigned remaining_ms,
                               unsigned increment_ms,
                               SearchLimits *limits);
int search_set_hash_size_mb(unsigned size_mb);

#endif /* SEARCH_H */
