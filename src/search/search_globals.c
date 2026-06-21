#include "search/search_internal.h"

/* Globals shared across search files */
FILE *search_log_output = NULL;
int stop_requested = 0;
unsigned hash_size = 16; /* default 16MB */
int lmr_base = 2;
int futility_margin = 114;
int rfp_margin_base = 111;
int nmp_min_depth = 2;
int singular_margin = 2;
int aspiration_window = 35;
int lmr_min_depth = 2;
int futility_max_depth = 4;
int lmr_history_divisor = 2000;
uint64_t node_count = 0;

Move killer_moves[2][MAX_DEPTH];
int history_scores[2][64][64];
Move countermoves[2][64][64];

int lmr_reductions[MAX_DEPTH][MAX_MOVES];
int lmr_initialized = 0;

TranspositionTable tt;
int tt_initialized = 0;

void init_lmr(void) {
  for (int depth = 0; depth < MAX_DEPTH; depth++) {
    for (int moves = 0; moves < MAX_MOVES; moves++) {
      if (depth == 0 || moves == 0) {
        lmr_reductions[depth][moves] = 0;
        continue;
      }
      double divisor = (double)lmr_base;
      double r;
      if (divisor <= 0.0) {
        r = 0.0;
      } else {
        r = 0.5 + log((double)depth) * log((double)moves) / divisor;
      }
      lmr_reductions[depth][moves] = (int)r;
    }
  }
  lmr_initialized = 1;
}

FILE *search_set_log_output(FILE *output) {
  FILE *prev = search_log_output;
  search_log_output = output;
  return prev;
}

void search_reset_stop_request(void) { stop_requested = 0; }

void search_request_stop(void) { stop_requested = 1; }

int search_set_hash_size_mb(unsigned size_mb) {
  hash_size = size_mb;
  if (tt_initialized) {
    transposition_table_free(&tt);
    tt_initialized = 0;
  }
  if (transposition_table_init(&tt, (size_t)hash_size * 1024 * 1024) == 0) {
    tt_initialized = 1;
  }
  return 0;
}

int search_set_lmr_base(int base) {
  if (base < 0 || base > 20) {
    return -1;
  }
  lmr_base = base;
  init_lmr();
  return 0;
}

int search_set_futility_margin(int margin) {
  if (margin < 0 || margin > 500) {
    return -1;
  }
  futility_margin = margin;
  return 0;
}

int search_set_rfp_margin(int margin) {
  if (margin < 0 || margin > 300) {
    return -1;
  }
  rfp_margin_base = margin;
  return 0;
}

int search_set_nmp_min_depth(int depth) {
  if (depth < 1 || depth > 10) {
    return -1;
  }
  nmp_min_depth = depth;
  return 0;
}

int search_set_singular_margin(int margin) {
  if (margin < 0 || margin > 10) {
    return -1;
  }
  singular_margin = margin;
  return 0;
}

int search_set_aspiration_window(int window) {
  if (window < 5 || window > 200) {
    return -1;
  }
  aspiration_window = window;
  return 0;
}

int search_set_lmr_min_depth(int depth) {
  if (depth < 1 || depth > 15) {
    return -1;
  }
  lmr_min_depth = depth;
  return 0;
}

int search_set_futility_max_depth(int depth) {
  if (depth < 1 || depth > 5) {
    return -1;
  }
  futility_max_depth = depth;
  return 0;
}

int search_set_lmr_history_divisor(int divisor) {
  if (divisor < 100 || divisor > 100000) {
    return -1;
  }
  lmr_history_divisor = divisor;
  return 0;
}

int search_get_history_score(Color side, Square from, Square to) {
  return history_scores[COLOR_IDX(side)][from][to];
}
