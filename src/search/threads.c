#include "search/threads.h"
#include "search/search_internal.h"

#include <stdlib.h>
#include <string.h>

/* Global thread pool */
ThreadPool thread_pool = { .threads = NULL, .num_threads = 0 };

/* Default thread count */
static int configured_threads = 1;

/* ── Helper thread entry point ───────────────────────────────────────────── */

/*
 * Helper threads (thread_id >= 1) run iterative deepening independently.
 * They share the transposition table with the main thread but use their own
 * killer/history/countermove tables. They do NOT report info lines.
 * They search until stop_requested is set by the main thread.
 */
static void *helper_thread_main(void *arg) {
    SearchThread *thread = (SearchThread *)arg;

    Position pos = thread->root_pos;

    if (use_nn && eval_nn) {
        nnue_refresh_accumulator(eval_nn, &pos);
    }

    /* Clear per-thread heuristics */
    memset(thread->killer_moves, 0, sizeof(thread->killer_moves));
    memset(thread->countermoves, 0, sizeof(thread->countermoves));
    for (int c = 0; c < 2; c++)
        for (int f = 0; f < 64; f++)
            for (int t = 0; t < 64; t++)
                thread->history_scores[c][f][t] = 0;

    thread->node_count = 0;
    thread->best_move = 0;
    thread->best_score = -INFINITY_SCORE;
    thread->depth_reached = 0;

    unsigned max_depth = thread->limits.depth;
    if (max_depth == 0 || max_depth > 64) {
        max_depth = 64;
    }

    Move best_move_so_far = thread->best_move_hint;
    int best_score_so_far = -INFINITY_SCORE;

    /*
     * Depth offset for thread diversity: helper threads start iterative
     * deepening at slightly different depths so they explore different
     * parts of the tree. This is a key part of Lazy SMP effectiveness.
     */
    unsigned start_depth = 1 + (unsigned)(thread->thread_id % 3);

    for (unsigned d = start_depth; d <= max_depth; d++) {
        PVLine pv;
        pv.length = 0;

        int score;
        if (d >= 3 && abs(best_score_so_far) < MATE_SCORE - 100) {
            score = search_aspiration_window(&pos, d, best_score_so_far, &pv,
                                             thread->start_time, &thread->limits,
                                             best_move_so_far, thread);
        } else {
            score = pvs(&pos, d, 0, -INFINITY_SCORE, INFINITY_SCORE, &pv,
                        thread->start_time, &thread->limits, best_move_so_far,
                        NULL, 1, 0, 0, thread);
        }

        if (atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
            break;
        }

        if (pv.length > 0) {
            best_move_so_far = pv.moves[0];
            best_score_so_far = score;
        }

        thread->best_move = best_move_so_far;
        thread->best_score = best_score_so_far;
        thread->depth_reached = d;
    }

    return NULL;
}

/* ── Thread pool management ──────────────────────────────────────────────── */

int threadpool_init(ThreadPool *pool, int num_threads) {
    if (pool == NULL || num_threads < 1) {
        return -1;
    }

    pool->threads = calloc((size_t)num_threads, sizeof(SearchThread));
    if (pool->threads == NULL) {
        pool->num_threads = 0;
        return -1;
    }

    pool->num_threads = num_threads;

    for (int i = 0; i < num_threads; i++) {
        pool->threads[i].thread_id = i;
        pool->threads[i].searching = 0;
    }

    return 0;
}

void threadpool_resize(ThreadPool *pool, int num_threads) {
    if (pool == NULL || num_threads < 1) {
        return;
    }

    /* Stop any active helpers first */
    threadpool_stop_helpers(pool);
    threadpool_wait_helpers(pool);

    if (pool->threads != NULL) {
        free(pool->threads);
    }

    pool->threads = calloc((size_t)num_threads, sizeof(SearchThread));
    if (pool->threads == NULL) {
        pool->num_threads = 0;
        return;
    }

    pool->num_threads = num_threads;
    for (int i = 0; i < num_threads; i++) {
        pool->threads[i].thread_id = i;
        pool->threads[i].searching = 0;
    }
}

void threadpool_free(ThreadPool *pool) {
    if (pool == NULL) {
        return;
    }

    threadpool_stop_helpers(pool);
    threadpool_wait_helpers(pool);

    free(pool->threads);
    pool->threads = NULL;
    pool->num_threads = 0;
}

void threadpool_start_helpers(ThreadPool *pool, const Position *pos,
                              const SearchLimits *limits, uint64_t start_time,
                              Move best_move_hint) {
    if (pool == NULL || pool->threads == NULL || pool->num_threads <= 1) {
        return; /* No helper threads to launch */
    }

    for (int i = 1; i < pool->num_threads; i++) {
        SearchThread *t = &pool->threads[i];

        t->root_pos = *pos;
        t->limits = *limits;
        t->start_time = start_time;
        t->best_move_hint = best_move_hint;
        t->searching = 1;

        if (pthread_create(&t->handle, NULL, helper_thread_main, t) != 0) {
            t->searching = 0;
        }
    }
}

void threadpool_stop_helpers(ThreadPool *pool) {
    if (pool == NULL) {
        return;
    }

    /* Signal all threads to stop via the shared atomic flag */
    atomic_store_explicit(&stop_requested, 1, memory_order_relaxed);
}

void threadpool_wait_helpers(ThreadPool *pool) {
    if (pool == NULL || pool->threads == NULL) {
        return;
    }

    for (int i = 1; i < pool->num_threads; i++) {
        if (pool->threads[i].searching) {
            pthread_join(pool->threads[i].handle, NULL);
            pool->threads[i].searching = 0;
        }
    }
}

uint64_t threadpool_total_nodes(const ThreadPool *pool) {
    if (pool == NULL || pool->threads == NULL) {
        return 0;
    }

    uint64_t total = 0;
    for (int i = 0; i < pool->num_threads; i++) {
        total += pool->threads[i].node_count;
    }
    return total;
}

int search_set_threads(int num_threads) {
    if (num_threads < 1 || num_threads > 64) {
        return -1;
    }
    configured_threads = num_threads;
    threadpool_resize(&thread_pool, num_threads);
    return 0;
}
