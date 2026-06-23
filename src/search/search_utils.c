#include "search/search_internal.h"

int search_compute_time_limits(const Position *board, unsigned depth,
                               unsigned remaining_ms, unsigned increment_ms,
                               unsigned movestogo, SearchLimits *limits) {
  (void)board;
  limits->depth = depth;
  limits->remaining_time_ms = remaining_ms;
  limits->increment_ms = increment_ms;
  limits->movestogo = movestogo;
  limits->is_time_controlled = 1;
  limits->soft_time_limit_ms = remaining_ms / 20;
  limits->hard_time_limit_ms = remaining_ms / 10 + increment_ms;
  return 0;
}

uint64_t get_time_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

int has_insufficient_material(const Position *pos) {
  if (pos->pieces[COLOR_IDX(WHITE)][PAWN] ||
      pos->pieces[COLOR_IDX(BLACK)][PAWN])
    return 0;
  if (pos->pieces[COLOR_IDX(WHITE)][ROOK] ||
      pos->pieces[COLOR_IDX(BLACK)][ROOK])
    return 0;
  if (pos->pieces[COLOR_IDX(WHITE)][QUEEN] ||
      pos->pieces[COLOR_IDX(BLACK)][QUEEN])
    return 0;

  int w_minors = bit_count(pos->pieces[COLOR_IDX(WHITE)][KNIGHT] |
                           pos->pieces[COLOR_IDX(WHITE)][BISHOP]);
  int b_minors = bit_count(pos->pieces[COLOR_IDX(BLACK)][KNIGHT] |
                           pos->pieces[COLOR_IDX(BLACK)][BISHOP]);

  if (w_minors <= 1 && b_minors <= 1) {
    return 1;
  }
  return 0;
}

int is_draw(const Position *pos) {
  if (pos->fiftyMoveCounter >= 100) {
    return 1;
  }
  if (has_insufficient_material(pos)) {
    return 1;
  }
  return 0;
}

int is_repetition(const Position *pos, const UndoNode *history,
                  const SearchLimits *limits) {
  uint64_t current_hash = pos->hashKey;
  const UndoNode *curr = history;
  while (curr != NULL) {
    if (curr->undo->old_hash == current_hash) {
      return 1;
    }
    PieceType pt = PIECE_TYPE(curr->undo->moving);
    if (pt == PAWN || curr->undo->captured != EMPTY) {
      break;
    }
    curr = curr->parent;
  }

  if (limits != NULL) {
    int check_count = pos->fiftyMoveCounter;
    if (check_count > limits->history_count) {
      check_count = limits->history_count;
    }
    for (int i = 0; i < check_count; i++) {
      int idx = limits->history_count - 1 - i;
      if (idx < 0)
        break;
      if (limits->history_hashes[idx] == current_hash) {
        return 1;
      }
    }
  }
  return 0;
}

int is_illegal_castle(const Position *pos, Move m) {
  int flag = MOVE_FLAG(m);
  if (flag == MOVE_CASTLE_KS || flag == MOVE_CASTLE_QS) {
    Color us = pos->sideToMove;
    int king_sq = pos->kingSq[COLOR_IDX(us)];
    if (is_square_attacked(pos, king_sq, OPPOSITE(us))) {
      return 1;
    }
    int step_sq = (flag == MOVE_CASTLE_KS) ? ((us == WHITE) ? F1 : F8)
                                           : ((us == WHITE) ? D1 : D8);
    if (is_square_attacked(pos, step_sq, OPPOSITE(us))) {
      return 1;
    }
  }
  return 0;
}

int make_move_and_check_legal(Position *pos, Move m, Undo *u) {
  apply_move(pos, m, u);
  Color us = OPPOSITE(pos->sideToMove); // the side that just moved
  int king_sq = pos->kingSq[COLOR_IDX(us)];
  if (is_square_attacked(pos, king_sq, pos->sideToMove)) {
    undo_move(pos, u);
    return 0; // Illegal
  }
  return 1; // Legal
}

int evaluate_no_moves(int in_check, int ply) {
  if (in_check) {
    return -MATE_SCORE + ply; // Checkmate
  }
  return 0; // Stalemate
}

void check_time_limit(uint64_t start_time, const SearchLimits *limits, SearchThread *thread) {
  thread->node_count++;
  if (thread->node_count % 2048 == 0) {
    if (limits->hard_time_limit_ms > 0) {
      uint64_t elapsed = get_time_ms() - start_time;
      if (elapsed >= limits->hard_time_limit_ms) {
        atomic_store_explicit(&stop_requested, 1, memory_order_relaxed);
      }
    }
  }
}
