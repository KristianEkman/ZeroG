#include "search/search.h"
#include "movegen/movegen.h"
#include <string.h>

static FILE *search_log_output = NULL;
static int stop_requested = 0;
static unsigned hash_size = 16; /* default 16MB */

FILE *search_set_log_output(FILE *output) {
    FILE *prev = search_log_output;
    search_log_output = output;
    return prev;
}

void search_reset_stop_request(void) {
    stop_requested = 0;
}

void search_request_stop(void) {
    stop_requested = 1;
}

int search_set_hash_size_mb(unsigned size_mb) {
    hash_size = size_mb;
    (void)hash_size;
    return 0;
}

int search_compute_time_limits(const Position *board,
                               unsigned depth,
                               unsigned remaining_ms,
                               unsigned increment_ms,
                               SearchLimits *limits) {
    (void)board;
    (void)depth;
    limits->soft_time_limit_ms = remaining_ms / 20;
    limits->hard_time_limit_ms = remaining_ms / 10 + increment_ms;
    return 0;
}

int search_best_move_with_limits(const Position *board, const SearchLimits *limits, SearchResult *result) {
    Move moves[MAX_MOVES];
    int count;

    (void)limits;
    count = movegen_legal(board, moves);

    result->node_count = 1;
    result->score = 0;
    
    if (count > 0) {
        result->best_move = moves[0];
        result->has_legal_move = 1;
    } else {
        result->best_move = 0;
        result->has_legal_move = 0;
    }

    if (search_log_output) {
        fprintf(search_log_output, "info depth 1 score cp 0 nodes 1\n");
        fflush(search_log_output);
    }

    return 0;
}
