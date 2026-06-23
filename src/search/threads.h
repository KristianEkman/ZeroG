#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

#include "boards.h"
#include "search/search.h"

/* Forward declarations — full definitions are in search_internal.h */
#define THREAD_MAX_DEPTH 64
#define THREAD_MAX_MOVES 256

/* ── Per-thread search state ─────────────────────────────────────────────── */
typedef struct SearchThread {
    int thread_id;

    /* Per-thread move ordering heuristics */
    Move killer_moves[2][THREAD_MAX_DEPTH];
    int  history_scores[2][64][64];
    Move countermoves[2][64][64];

    /* Per-thread node counter */
    uint64_t node_count;

    /* Root position and limits (copies for each thread) */
    Position root_pos;
    SearchLimits limits;
    uint64_t start_time;
    Move best_move_hint;      /* PV move from previous iteration */

    /* Result from this thread's search */
    Move best_move;
    int  best_score;
    unsigned depth_reached;

    /* Thread control */
    pthread_t handle;
    int searching;
} SearchThread;

/* ── Thread pool ─────────────────────────────────────────────────────────── */
typedef struct {
    SearchThread *threads;
    int num_threads;             /* Total number of threads (including main = thread 0) */
} ThreadPool;

/* Global thread pool instance */
extern ThreadPool thread_pool;

/* Initialization / cleanup */
int  threadpool_init(ThreadPool *pool, int num_threads);
void threadpool_resize(ThreadPool *pool, int num_threads);
void threadpool_free(ThreadPool *pool);

/* Search control */
void threadpool_start_helpers(ThreadPool *pool, const Position *pos,
                              const SearchLimits *limits, uint64_t start_time,
                              Move best_move_hint);
void threadpool_stop_helpers(ThreadPool *pool);
void threadpool_wait_helpers(ThreadPool *pool);

/* Aggregate node count across all threads */
uint64_t threadpool_total_nodes(const ThreadPool *pool);

/* UCI option */
int search_set_threads(int num_threads);

#endif /* THREADS_H */
