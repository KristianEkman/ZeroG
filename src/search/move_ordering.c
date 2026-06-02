#include "search/search_internal.h"

int move_is_capture_or_promo(const Position *pos, Move m) {
  int to = MOVE_TO(m);
  if (pos->board[to] != EMPTY) {
    return 1;
  }
  int from = MOVE_FROM(m);
  Piece moving_piece = pos->board[from];
  PieceType pt = PIECE_TYPE(moving_piece);
  if (pt == PAWN && to == pos->enPassantSquare) {
    return 1;
  }
  if (pt == PAWN && (RANK_OF(to) == 0 || RANK_OF(to) == 7)) {
    return 1;
  }
  return 0;
}

int score_move(const Position *pos, Move m, Move pv_move, int ply) {
  if (m == pv_move) {
    return 1000000;
  }

  int from = MOVE_FROM(m);
  int to = MOVE_TO(m);
  Piece moving_piece = pos->board[from];
  PieceType attacker = PIECE_TYPE(moving_piece);

  // Is it a capture?
  Piece victim_piece = pos->board[to];
  int is_cap = (victim_piece != EMPTY);
  if (!is_cap && attacker == PAWN && to == pos->enPassantSquare) {
    victim_piece = MAKE_PIECE(OPPOSITE(pos->sideToMove), PAWN);
    is_cap = 1;
  }

  if (is_cap) {
    int see_val = see(pos, m);
    if (see_val >= 0) {
      return 100000 + see_val;
    } else {
      return 35000 + see_val;
    }
  }

  // Promotions
  if (attacker == PAWN && (RANK_OF(to) == 0 || RANK_OF(to) == 7)) {
    return 50000 + MOVE_PROMO(m);
  }

  // Killer moves
  if (m == killer_moves[0][ply]) {
    return 90000;
  }
  if (m == killer_moves[1][ply]) {
    return 80000;
  }

  // Castling moves
  int flag = MOVE_FLAG(m);
  if (flag == MOVE_CASTLE_KS || flag == MOVE_CASTLE_QS) {
    return 10000;
  }

  return history_scores[COLOR_IDX(pos->sideToMove)][from][to];
}

void pick_best_move(Move *moves, int *scores, int count, int current_idx) {
  int best_idx = current_idx;
  for (int j = current_idx + 1; j < count; j++) {
    if (scores[j] > scores[best_idx]) {
      best_idx = j;
    }
  }
  if (best_idx != current_idx) {
    Move temp_move = moves[current_idx];
    moves[current_idx] = moves[best_idx];
    moves[best_idx] = temp_move;

    int temp_score = scores[current_idx];
    scores[current_idx] = scores[best_idx];
    scores[best_idx] = temp_score;
  }
}

void update_quiet_move_heuristics(const Position *pos, Move cut_move, Move *moves, int tried_count, int depth, int ply) {
  if (!move_is_capture_or_promo(pos, cut_move)) {
    if (killer_moves[0][ply] != cut_move) {
      killer_moves[1][ply] = killer_moves[0][ply];
      killer_moves[0][ply] = cut_move;
    }

    int from = MOVE_FROM(cut_move);
    int to = MOVE_TO(cut_move);
    int bonus = depth * depth;
    if (bonus > 2000) {
      bonus = 2000;
    }
    int color_idx = COLOR_IDX(pos->sideToMove);
    history_scores[color_idx][from][to] +=
        bonus - history_scores[color_idx][from][to] * bonus / 20000;

    for (int j = 0; j < tried_count; j++) {
      if (moves[j] == cut_move) {
        break;
      }
      if (!move_is_capture_or_promo(pos, moves[j])) {
        int prev_from = MOVE_FROM(moves[j]);
        int prev_to = MOVE_TO(moves[j]);
        history_scores[color_idx][prev_from][prev_to] -=
            bonus + history_scores[color_idx][prev_from][prev_to] * bonus / 20000;
      }
    }
  }
}
