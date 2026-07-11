#include "search/search_internal.h"
#include "nn.h"
#include "search/book.h"

int reconstruct_pv_from_tt(const Position *pos, PVLine *pv, int max_depth) {
  Position temp_pos = *pos;
  pv->length = 0;

  uint64_t visited_keys[MAX_DEPTH];
  int visited_count = 0;

  for (int ply = 0; ply < max_depth && ply < MAX_DEPTH; ply++) {
    TranspositionEntry entry;
    int hit = transposition_table_lookup(&tt, temp_pos.hashKey, ply, &entry);
    if (hit != 1 || entry.best_move == 0) {
      break;
    }

    // Verify that entry.best_move is legal in the current position
    Move moves[MAX_MOVES];
    int count = movegen_legal(&temp_pos, moves);
    int legal = 0;
    for (int i = 0; i < count; i++) {
      if (moves[i] == entry.best_move) {
        legal = 1;
        break;
      }
    }
    if (!legal) {
      break;
    }

    // Detect transposition/repetition cycles
    int cycle = 0;
    for (int i = 0; i < visited_count; i++) {
      if (visited_keys[i] == temp_pos.hashKey) {
        cycle = 1;
        break;
      }
    }
    if (cycle) {
      break;
    }
    visited_keys[visited_count++] = temp_pos.hashKey;

    pv->moves[ply] = entry.best_move;
    pv->length = ply + 1;

    Undo u;
    apply_move(&temp_pos, entry.best_move, &u);
  }
  return pv->length;
}

void log_search_info(unsigned depth, int score, uint64_t nodes,
                            const PVLine *pv, const Position *board, uint64_t start_time) {
  if (!search_log_output) {
    return;
  }
  uint64_t elapsed_ms = get_time_ms() - start_time;
  if (elapsed_ms == 0) elapsed_ms = 1;
  uint64_t nps = (nodes * 1000) / elapsed_ms;

  fprintf(search_log_output, "info depth %u score ", depth);
  if (score > MATE_SCORE - 100) {
    int plies = MATE_SCORE - score;
    int moves_to_mate = (plies + 1) / 2;
    fprintf(search_log_output, "mate %d ", moves_to_mate);
  } else if (score < -MATE_SCORE + 100) {
    int plies = -MATE_SCORE - score;
    int moves_to_mate = (plies - 1) / 2;
    fprintf(search_log_output, "mate %d ", moves_to_mate);
  } else {
    fprintf(search_log_output, "cp %d ", score);
  }
  fprintf(search_log_output, "time %llu nodes %llu nps %llu pv",
          (unsigned long long)elapsed_ms,
          (unsigned long long)nodes,
          (unsigned long long)nps);
  Position temp_pos = *board;
  for (int i = 0; i < pv->length; i++) {
    char move_str[6];
    if (uci_move_to_string(&temp_pos, pv->moves[i], move_str,
                           sizeof(move_str)) == 0) {
      fprintf(search_log_output, " %s", move_str);
    }
    Undo u;
    apply_move(&temp_pos, pv->moves[i], &u);
  }
  fprintf(search_log_output, "\n");
  fflush(search_log_output);
}

int search_best_move_with_limits(const Position *board,
                                 const SearchLimits *limits,
                                 SearchResult *result) {
  search_reset_stop_request();
  result->depth = 0;
  uint64_t start_time = get_time_ms();

  /* Probe opening book if enabled */
  if (use_book && book_path[0] != '\0') {
      Move book_move = book_probe(board);
      if (book_move != 0) {
          result->best_move = book_move;
          result->has_legal_move = 1;
          result->score = 0;
          result->depth = 0;
          result->node_count = 0;
          if (search_log_output != NULL) {
              fprintf(search_log_output, "info string book move found\n");
              fflush(search_log_output);
          }
          return 0;
      }
  }

  if (!lmr_initialized) {
    init_lmr();
  }

  if (!tt_initialized) {
    if (transposition_table_init(&tt, (size_t)hash_size * 1024 * 1024) == 0) {
      tt_initialized = 1;
    }
  }
  if (tt_initialized) {
    transposition_table_next_generation(&tt);
  }

  /* Ensure thread pool is initialized (at least 1 thread) */
  if (thread_pool.threads == NULL || thread_pool.num_threads < 1) {
    threadpool_init(&thread_pool, 1);
  }

  /* Use thread 0 (main thread) for the primary search */
  SearchThread *main_thread = &thread_pool.threads[0];
  main_thread->node_count = 0;

  /* Clear per-thread heuristics for main thread */
  memset(main_thread->killer_moves, 0, sizeof(main_thread->killer_moves));
  memset(main_thread->countermoves, 0, sizeof(main_thread->countermoves));
  // Age history scores (halve) instead of clearing — preserves learned move ordering
  for (int c = 0; c < 2; c++)
    for (int f = 0; f < 64; f++)
      for (int t = 0; t < 64; t++)
        main_thread->history_scores[c][f][t] /= 2;

  Position pos = *board;
  if (use_nn && eval_nn) {
      nnue_refresh_accumulator(eval_nn, &pos);
  }
  Move moves[MAX_MOVES];
  int count = movegen_legal(&pos, moves);

  if (count == 0) {
    result->best_move = 0;
    result->score = 0;
    result->has_legal_move = 0;
    result->node_count = 0;
    return 0;
  }

  // Default fallbacks
  result->best_move = moves[0];
  result->has_legal_move = 1;
  result->score = 0;
  result->node_count = 0;

  Move best_move_so_far = moves[0];
  int best_score_so_far = -INFINITY_SCORE;

  unsigned max_search_depth = limits->depth;
  if (max_search_depth == 0 || max_search_depth > 64) {
    max_search_depth = 64;
  }

  SearchLimits local_limits = *limits;
  TimeControl tc;
  int use_tc = limits->is_time_controlled;
  if (use_tc) {
    time_control_init(&tc, board, limits->remaining_time_ms,
                      limits->increment_ms, limits->movestogo, count);
    local_limits.soft_time_limit_ms = tc.soft_limit_ms;
    local_limits.hard_time_limit_ms = tc.hard_limit_ms;
  }

  /* Launch helper threads (thread_id >= 1) for Lazy SMP */
  threadpool_start_helpers(&thread_pool, board, &local_limits, start_time,
                           best_move_so_far);

  // Iterative Deepening (main thread)
  for (unsigned d = 1; d <= max_search_depth; d++) {
    if (local_limits.soft_time_limit_ms > 0) {
      uint64_t elapsed = get_time_ms() - start_time;
      if (elapsed >= local_limits.soft_time_limit_ms) {
        break;
      }
    }

    PVLine pv;
    pv.length = 0;

    int score;
    if (d >= 3 && abs(best_score_so_far) < MATE_SCORE - 100) {
      score =
          search_aspiration_window(&pos, d, best_score_so_far, &pv, start_time,
                                   &local_limits, best_move_so_far, main_thread);
    } else {
      score = pvs(&pos, d, 0, -INFINITY_SCORE, INFINITY_SCORE, &pv, start_time,
                  &local_limits, best_move_so_far, NULL, 1, 0, 0, main_thread);
    }

    if (atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
      break;
    }

    if (pv.length > 0) {
      best_move_so_far = pv.moves[0];
      best_score_so_far = score;

      result->best_move = best_move_so_far;
      result->score = best_score_so_far;
      result->node_count = threadpool_total_nodes(&thread_pool);
    }

    result->depth = d;

    // Reconstruct the PV from the transposition table to get a complete PV trace
    PVLine tt_pv;
    memset(&tt_pv, 0, sizeof(tt_pv));
    reconstruct_pv_from_tt(&pos, &tt_pv, MAX_DEPTH);
    if (tt_pv.length > 0) {
      pv = tt_pv;
    }

    log_search_info(d, score, threadpool_total_nodes(&thread_pool), &pv, board, start_time);

    if (use_tc) {
      uint64_t elapsed = get_time_ms() - start_time;
      if (time_control_update(&tc, d, score, best_move_so_far,
                              threadpool_total_nodes(&thread_pool),
                              elapsed)) {
        break;
      }
      local_limits.soft_time_limit_ms = tc.soft_limit_ms;
      local_limits.hard_time_limit_ms = tc.hard_limit_ms;
    }
  }

  /* Stop and wait for helper threads */
  threadpool_stop_helpers(&thread_pool);
  threadpool_wait_helpers(&thread_pool);

  /* Final node count aggregation */
  result->node_count = threadpool_total_nodes(&thread_pool);

  /* Reset stop flag for next search */
  search_reset_stop_request();

  return 0;
}

int search_run_quiescence_only(const Position *board) {
  Position pos = *board;
  search_reset_stop_request();

  /* Ensure thread pool is initialized */
  if (thread_pool.threads == NULL || thread_pool.num_threads < 1) {
    threadpool_init(&thread_pool, 1);
  }
  SearchThread *main_thread = &thread_pool.threads[0];
  memset(main_thread->killer_moves, 0, sizeof(main_thread->killer_moves));
  main_thread->node_count = 0;

  SearchLimits limits = {0};
  limits.hard_time_limit_ms = 0;
  limits.is_time_controlled = 0;
  return quiescence(&pos, 0, -INFINITY_SCORE, INFINITY_SCORE, 0, &limits, main_thread);
}
