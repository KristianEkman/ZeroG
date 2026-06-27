#include "search/search_internal.h"

static inline int is_passed_pawn(const Position *pos, Square sq, Color side) {
  Color opponent = OPPOSITE(side);
  uint64_t opp_pawns = pos->pieces[COLOR_IDX(opponent)][PAWN];
  uint64_t mask = passedPawnMasks[COLOR_IDX(side)][sq];
  return (mask & opp_pawns) == 0;
}

int try_null_move_pruning(Position *pos, int depth, int ply, int beta,
                          uint64_t start_time, const SearchLimits *limits,
                          const UndoNode *history, SearchThread *thread) {
  uint64_t non_pawns = pos->pieces[COLOR_IDX(pos->sideToMove)][KNIGHT] |
                       pos->pieces[COLOR_IDX(pos->sideToMove)][BISHOP] |
                       pos->pieces[COLOR_IDX(pos->sideToMove)][ROOK] |
                       pos->pieces[COLOR_IDX(pos->sideToMove)][QUEEN];
  if (non_pawns == 0) {
    return -INFINITY_SCORE;
  }

  int R = (depth > 6) ? 3 : 2;

  // Make Null Move
  int old_ep = pos->enPassantSquare;
  int old_fifty = pos->fiftyMoveCounter;
  uint64_t old_hash = pos->hashKey;

  pos->sideToMove = OPPOSITE(pos->sideToMove);
  pos->hashKey ^= zobrist_side_to_move_key();
  if (pos->enPassantSquare != SQ_NONE) {
    pos->hashKey ^= zobrist_en_passant_key(pos->enPassantSquare);
    pos->enPassantSquare = SQ_NONE;
  }
  pos->fiftyMoveCounter++;

  PVLine child_pv;
  child_pv.length = 0;

  // Search null move with a reduced depth and zero window
  int score = -pvs(pos, depth - 1 - R, ply + 1, -beta, -beta + 1, &child_pv,
                   start_time, limits, 0, history, 0, 0, 0, thread);

  // Unmake Null Move
  pos->enPassantSquare = old_ep;
  pos->fiftyMoveCounter = old_fifty;
  pos->sideToMove = OPPOSITE(pos->sideToMove);
  pos->hashKey = old_hash;

  return score;
}

int quiescence(Position *pos, int ply, int alpha, int beta, uint64_t start_time,
               const SearchLimits *limits, SearchThread *thread) {
  if (ply >= MAX_DEPTH - 1) {
    int eval = evaluate(pos);
    return pos->sideToMove == WHITE ? eval : -eval;
  }

  check_time_limit(start_time, limits, thread);
  if (atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
    return 0;
  }

  int stand_pat = evaluate(pos);
  if (pos->sideToMove == BLACK) {
    stand_pat = -stand_pat;
  }

  if (stand_pat >= beta) {
    return beta;
  }
  if (stand_pat > alpha) {
    alpha = stand_pat;
  }

  Move moves[MAX_MOVES];
  int count = 0;
  int in_check = is_square_attacked(
      pos, pos->kingSq[COLOR_IDX(pos->sideToMove)], OPPOSITE(pos->sideToMove));

  if (in_check) {
    count = movegen_pseudo_legal(pos, moves);
  } else {
    Move all_moves[MAX_MOVES];
    int all_count = movegen_pseudo_legal(pos, all_moves);
    for (int i = 0; i < all_count; i++) {
      if (move_is_capture_or_promo(pos, all_moves[i])) {
        moves[count++] = all_moves[i];
      }
    }
  }

  int scores[MAX_MOVES];
  for (int i = 0; i < count; i++) {
    scores[i] = score_move(pos, moves[i], 0, ply, 0, thread);
  }

  int legal_moves_searched = 0;

  for (int i = 0; i < count; i++) {
    pick_best_move(moves, scores, count, i);
    if (is_illegal_castle(pos, moves[i])) {
      continue;
    }

    // SEE pruning: prune bad captures in quiescence search if not in check
    if (!in_check) {
      int to = MOVE_TO(moves[i]);
      if (pos->board[to] != EMPTY || to == pos->enPassantSquare) {
        int from = MOVE_FROM(moves[i]);
        int is_promo = (PIECE_TYPE(pos->board[from]) == PAWN) &&
                       (RANK_OF(to) == 0 || RANK_OF(to) == 7);
        if (!is_promo && see(pos, moves[i]) < 0) {
          continue;
        }
      }
    }

    Undo u;
    if (!make_move_and_check_legal(pos, moves[i], &u)) {
      continue;
    }

    legal_moves_searched++;
    int score = -quiescence(pos, ply + 1, -beta, -alpha, start_time, limits, thread);
    undo_move(pos, &u);

    if (atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
      return 0;
    }

    if (score >= beta) {
      return beta;
    }
    if (score > alpha) {
      alpha = score;
    }
  }

  if (in_check && legal_moves_searched == 0) {
    return -MATE_SCORE + ply;
  }

  return alpha;
}

int pvs(Position *pos, int depth, int ply, int alpha, int beta, PVLine *pv,
        uint64_t start_time, const SearchLimits *limits, Move pv_move,
        const UndoNode *history, int allow_nmp, Move excluded_move,
        Move prev_move, SearchThread *thread) {
  if (ply >= MAX_DEPTH - 1) {
    pv->length = 0;
    int eval = evaluate(pos);
    return pos->sideToMove == WHITE ? eval : -eval;
  }

  check_time_limit(start_time, limits, thread);
  if (atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
    return 0;
  }

  if (ply > 0 && (is_draw(pos) || is_repetition(pos, history, limits))) {
    pv->length = 0;
    return 0;
  }

  // Mate distance pruning
  int mated_score = -MATE_SCORE + ply;
  int mate_score = MATE_SCORE - ply;
  if (alpha < mated_score)
    alpha = mated_score;
  if (beta > mate_score)
    beta = mate_score;
  if (alpha >= beta)
    return alpha;

  int old_alpha = alpha;
  TranspositionEntry tt_entry = {0};
  int tt_hit = 0;
  if (excluded_move == 0) {
    tt_hit = transposition_table_lookup(&tt, pos->hashKey, ply, &tt_entry);
    if (tt_hit == 1) {
      if (tt_entry.depth >= (unsigned)depth) {
        if (tt_entry.bound == TT_BOUND_EXACT) {
          pv->length = 0;
          if (tt_entry.best_move != 0) {
            pv->moves[0] = tt_entry.best_move;
            pv->length = 1;
          }
          return tt_entry.score;
        } else if (tt_entry.bound == TT_BOUND_LOWER && tt_entry.score >= beta) {
          return tt_entry.score;
        } else if (tt_entry.bound == TT_BOUND_UPPER &&
                   tt_entry.score <= alpha) {
          return tt_entry.score;
        }
      }
    }
  }

  if (depth <= 0) {
    pv->length = 0;
    return quiescence(pos, ply, alpha, beta, start_time, limits, thread);
  }

  int in_check = is_square_attacked(
      pos, pos->kingSq[COLOR_IDX(pos->sideToMove)], OPPOSITE(pos->sideToMove));

  if (in_check) {
    depth++;
  }

  int static_eval = 0;
  if (!in_check) {
    static_eval = evaluate(pos);
    if (pos->sideToMove == BLACK) {
      static_eval = -static_eval;
    }
  }

  // Reverse Futility Pruning (RFP)
  if (ply > 0 && !in_check && depth <= 3 && beta < MATE_SCORE - 100) {
    int rfp_margin = rfp_margin_base * depth;
    if (static_eval - rfp_margin >= beta) {
      return static_eval;
    }
  }

  // Null Move Pruning (NMP)
  if (allow_nmp && !in_check && depth >= nmp_min_depth) {
    int score = try_null_move_pruning(pos, depth, ply, beta, start_time, limits,
                                      history, thread);
    if (atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
      return 0;
    }
    if (score >= beta) {
      if (score >= MATE_SCORE - 100) {
        return beta;
      }
      return score;
    }
  }

  int extension = 0;
  Move hash_move = 0;
  if (tt_hit == 1 && tt_entry.best_move != 0) {
    hash_move = tt_entry.best_move;
  } else if (ply == 0) {
    hash_move = pv_move;
  }

  // Singular Extension check
  if (excluded_move == 0 && depth >= 8 && hash_move != 0 && tt_hit == 1 &&
      tt_entry.depth >= (unsigned)(depth - 3) &&
      tt_entry.bound != TT_BOUND_UPPER &&
      abs(tt_entry.score) < MATE_SCORE - 100) {

    int r = 3;
    int margin = singular_margin * depth;
    int singular_beta = tt_entry.score - margin;
    PVLine child_pv;
    child_pv.length = 0;

    int score = pvs(pos, depth - 1 - r, ply, singular_beta - 1, singular_beta,
                    &child_pv, start_time, limits, 0, history, 0, hash_move, prev_move, thread);
    if (score < singular_beta && !atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
      extension = 1;
    }
  }

  Move moves[MAX_MOVES];
  int count = movegen_pseudo_legal(pos, moves);

  // One-Reply Extension check
  int is_one_reply = 0;
  if (depth > 0 && ply > 0) {
    int legal_count = 0;
    for (int i = 0; i < count; i++) {
      if (is_illegal_castle(pos, moves[i])) {
        continue;
      }
      Undo u;
      if (make_move_and_check_legal(pos, moves[i], &u)) {
        legal_count++;
        undo_move(pos, &u);
        if (legal_count > 1) {
          break;
        }
      }
    }
    if (legal_count == 1) {
      is_one_reply = 1;
    }
  }

  int scores[MAX_MOVES];
  for (int i = 0; i < count; i++) {
    scores[i] = score_move(pos, moves[i], hash_move, ply, prev_move, thread);
  }

  int best_score = -INFINITY_SCORE;
  Move best_move = 0;
  int search_pv = 1;
  int legal_moves_searched = 0;

  for (int i = 0; i < count; i++) {
    pick_best_move(moves, scores, count, i);
    if (moves[i] == excluded_move) {
      continue;
    }

    PVLine child_pv;
    child_pv.length = 0;

    if (is_illegal_castle(pos, moves[i])) {
      continue;
    }

    int ext = (moves[i] == hash_move && extension == 1) ? 1 : 0;

    // Passed Pawn Push Extension
    if (ext == 0) {
      int from = MOVE_FROM(moves[i]);
      int to = MOVE_TO(moves[i]);
      PieceType pt = PIECE_TYPE(pos->board[from]);
      if (pt == PAWN) {
        int tr = RANK_OF(to);
        Color side = pos->sideToMove;
        if ((side == WHITE && tr == RANK_7) || (side == BLACK && tr == RANK_2)) {
          if (is_passed_pawn(pos, from, side)) {
            ext = 1;
          }
        }
      }
    }

    // One-Reply Extension
    if (ext == 0 && is_one_reply) {
      ext = 1;
    }

    int is_quiet = !move_is_capture_or_promo(pos, moves[i]);

    int reduction = 0;
    int is_pv = (beta - alpha > 1);

    if (depth >= lmr_min_depth && legal_moves_searched >= 1 && is_quiet &&
        !in_check) {
      int move_count = legal_moves_searched + 1;
      reduction =
          lmr_reductions[depth >= MAX_DEPTH ? MAX_DEPTH - 1 : depth]
                        [move_count >= MAX_MOVES ? MAX_MOVES - 1 : move_count];
      if (is_pv) {
        reduction--;
      }
      if (moves[i] == thread->killer_moves[0][ply] ||
          moves[i] == thread->killer_moves[1][ply]) {
        reduction--;
      }

      int hist = thread->history_scores[COLOR_IDX(pos->sideToMove)][MOVE_FROM(moves[i])]
                               [MOVE_TO(moves[i])];
      int hist_adj = (hist + (hist > 0 ? lmr_history_divisor / 2
                                       : -lmr_history_divisor / 2)) /
                     lmr_history_divisor;
      reduction -= hist_adj;

      if (reduction < 0) {
        reduction = 0;
      } else if (reduction >= depth) {
        reduction = depth - 1;
      }
    }

    // Late Move Pruning: skip late quiet moves at shallow depths
    if (!in_check && is_quiet && depth <= 4 && !is_pv) {
      static const int lmp_counts[] = {0, 5, 9, 14, 21};
      if (legal_moves_searched >= lmp_counts[depth]) {
        continue;
      }
    }

    Undo u;
    if (!make_move_and_check_legal(pos, moves[i], &u)) {
      continue;
    }

    legal_moves_searched++;

    int gives_check =
        is_square_attacked(pos, pos->kingSq[COLOR_IDX(pos->sideToMove)],
                           OPPOSITE(pos->sideToMove));
    if (gives_check) {
      reduction = 0;
    }

    // Futility Pruning
    if (depth <= futility_max_depth && !in_check && is_quiet && !gives_check &&
        legal_moves_searched > 1 && (alpha < MATE_SCORE - 100)) {
      int fp_margin = futility_margin * depth;
      if (static_eval + fp_margin <= alpha) {
        undo_move(pos, &u);
        continue;
      }
    }

    UndoNode next_node;
    next_node.undo = &u;
    next_node.parent = history;

    int score;
    if (search_pv) {
      score = -pvs(pos, depth - 1 + ext, ply + 1, -beta, -alpha, &child_pv,
                   start_time, limits, 0, &next_node, 1, 0, moves[i], thread);
      search_pv = 0;
    } else {
      if (reduction > 0) {
        score =
            -pvs(pos, depth - 1 - reduction + ext, ply + 1, -alpha - 1, -alpha,
                 &child_pv, start_time, limits, 0, &next_node, 1, 0, moves[i], thread);
        if (score > alpha && !atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
          score = -pvs(pos, depth - 1 + ext, ply + 1, -alpha - 1, -alpha,
                       &child_pv, start_time, limits, 0, &next_node, 1, 0, moves[i], thread);
        }
      } else {
        score = -pvs(pos, depth - 1 + ext, ply + 1, -alpha - 1, -alpha,
                     &child_pv, start_time, limits, 0, &next_node, 1, 0, moves[i], thread);
      }

      if (score > alpha && score < beta && !atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
        score = -pvs(pos, depth - 1 + ext, ply + 1, -beta, -alpha, &child_pv,
                     start_time, limits, 0, &next_node, 1, 0, moves[i], thread);
      }
    }

    undo_move(pos, &u);

    if (atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
      return 0;
    }

    if (score > best_score) {
      best_score = score;
      best_move = moves[i];
    }

    if (score > alpha) {
      alpha = score;

      pv->moves[0] = moves[i];
      memcpy(pv->moves + 1, child_pv.moves, child_pv.length * sizeof(Move));
      pv->length = child_pv.length + 1;
    }

    if (alpha >= beta) {
      update_quiet_move_heuristics(pos, moves[i], moves, i, depth, ply, prev_move, thread);
      break;
    }
  }

  if (legal_moves_searched == 0) {
    pv->length = 0;
    best_score = evaluate_no_moves(in_check, ply);
  }

  if (!atomic_load_explicit(&stop_requested, memory_order_relaxed) && excluded_move == 0) {
    TranspositionBound bound = TT_BOUND_EXACT;
    if (best_score <= old_alpha) {
      bound = TT_BOUND_UPPER;
    } else if (best_score >= beta) {
      bound = TT_BOUND_LOWER;
    }
    transposition_table_store(&tt, pos->hashKey, depth, ply, best_score, bound,
                              best_move);
  }

  return best_score;
}

int search_aspiration_window(Position *pos, unsigned depth, int previous_score,
                             PVLine *pv, uint64_t start_time,
                             const SearchLimits *limits,
                             Move best_move_so_far, SearchThread *thread) {
  int delta = aspiration_window; // centipawns window
  int alpha = previous_score - delta;
  int beta = previous_score + delta;
  int score = previous_score;

  while (1) {
    if (alpha < -INFINITY_SCORE)
      alpha = -INFINITY_SCORE;
    if (beta > INFINITY_SCORE)
      beta = INFINITY_SCORE;

    score = pvs(pos, depth, 0, alpha, beta, pv, start_time, limits,
                best_move_so_far, NULL, 1, 0, 0, thread);

    if (atomic_load_explicit(&stop_requested, memory_order_relaxed)) {
      break;
    }

    if (score <= alpha) {
      // Fail low: widen alpha
      alpha -= delta;
      delta *= 2;
    } else if (score >= beta) {
      // Fail high: widen beta
      beta += delta;
      delta *= 2;
    } else {
      break;
    }
  }
  return score;
}
