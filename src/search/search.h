#ifndef SEARCH_H
#define SEARCH_H

#include <stdio.h>
#include "boards.h"

typedef struct {
    unsigned depth;
    unsigned soft_time_limit_ms;
    unsigned hard_time_limit_ms;
    unsigned remaining_time_ms;
    unsigned increment_ms;
    unsigned movestogo;
    int is_time_controlled;
    uint64_t history_hashes[1024];
    int history_count;
} SearchLimits;

typedef struct {
    Move best_move;
    int score;
    int has_legal_move;
    uint64_t node_count;
    unsigned depth;
} SearchResult;

FILE *search_set_log_output(FILE *output);
int search_best_move_with_limits(const Position *board, const SearchLimits *limits, SearchResult *result);
void search_reset_stop_request(void);
void search_request_stop(void);
int search_compute_time_limits(const Position *board,
                               unsigned depth,
                               unsigned remaining_ms,
                               unsigned increment_ms,
                               unsigned movestogo,
                               SearchLimits *limits);
int search_set_hash_size_mb(unsigned size_mb);
int search_set_lmr_base(int base);
int search_set_futility_margin(int margin);
int search_set_rfp_margin(int margin);
int search_set_nmp_min_depth(int depth);
int search_set_singular_margin(int margin);
int search_set_aspiration_window(int window);
int search_set_lmr_min_depth(int depth);
int search_set_futility_max_depth(int depth);
int search_set_lmr_history_divisor(int divisor);
int search_set_threads(int num_threads);
int search_get_history_score(Color side, Square from, Square to);
int search_run_quiescence_only(const Position *board);

#endif /* SEARCH_H */
