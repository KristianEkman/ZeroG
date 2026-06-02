#include "search/search_internal.h"

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
                            const PVLine *pv, const Position *board) {
  if (!search_log_output) {
    return;
  }
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
  fprintf(search_log_output, "nodes %llu pv", (unsigned long long)nodes);
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
  node_count = 0;
  uint64_t start_time = get_time_ms();

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

  memset(killer_moves, 0, sizeof(killer_moves));
  memset(history_scores, 0, sizeof(history_scores));

  Position pos = *board;
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

  // Iterative Deepening
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
                                   &local_limits, best_move_so_far);
    } else {
      score = pvs(&pos, d, 0, -INFINITY_SCORE, INFINITY_SCORE, &pv, start_time,
                  &local_limits, best_move_so_far, NULL, 1, 0);
    }

    if (stop_requested) {
      break;
    }

    if (pv.length > 0) {
      best_move_so_far = pv.moves[0];
      best_score_so_far = score;

      result->best_move = best_move_so_far;
      result->score = best_score_so_far;
      result->node_count = node_count;
    }

    // Reconstruct the PV from the transposition table to get a complete PV trace
    PVLine tt_pv;
    memset(&tt_pv, 0, sizeof(tt_pv));
    reconstruct_pv_from_tt(&pos, &tt_pv, MAX_DEPTH);
    if (tt_pv.length > 0) {
      pv = tt_pv;
    }

    log_search_info(d, score, node_count, &pv, board);

    if (use_tc) {
      uint64_t elapsed = get_time_ms() - start_time;
      if (time_control_update(&tc, d, score, best_move_so_far, node_count,
                              elapsed)) {
        break;
      }
      local_limits.soft_time_limit_ms = tc.soft_limit_ms;
      local_limits.hard_time_limit_ms = tc.hard_limit_ms;
    }
  }

  return 0;
}

int search_run_quiescence_only(const Position *board) {
  Position pos = *board;
  search_reset_stop_request();
  memset(killer_moves, 0, sizeof(killer_moves));
  SearchLimits limits = {0};
  limits.hard_time_limit_ms = 0;
  limits.is_time_controlled = 0;
  return quiescence(&pos, 0, -INFINITY_SCORE, INFINITY_SCORE, 0, &limits);
}
