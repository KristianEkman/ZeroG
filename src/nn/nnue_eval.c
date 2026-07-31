#include "nn.h"
#include "movegen.h"
#include <stdio.h>
#include <string.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

void nn_extract_features(const Position *pos, float *features) {
  if (!pos || !features)
    return;

  memset(features, 0, NN_INPUT_SIZE * sizeof(float));
  Color stm = pos->sideToMove;
  int ksq = pos->kingSq[COLOR_IDX(stm)];

  for (int sq = 0; sq < 64; sq++) {
    Piece p = pos->board[sq];
    if (p == EMPTY)
      continue;

    int feature_idx = nnue_feature_index(stm, p, sq, ksq);
    if (feature_idx >= 0 && feature_idx < 40960) {
      features[feature_idx] = 1.0f;
    }
  }

  // Add castling features (stm perspective-relative)
  if (stm == WHITE) {
    if (pos->castlingRights & CASTLE_WK)
      features[40960] = 1.0f;
    if (pos->castlingRights & CASTLE_WQ)
      features[40961] = 1.0f;
    if (pos->castlingRights & CASTLE_BK)
      features[40962] = 1.0f;
    if (pos->castlingRights & CASTLE_BQ)
      features[40963] = 1.0f;
  } else {
    if (pos->castlingRights & CASTLE_BK)
      features[40960] = 1.0f;
    if (pos->castlingRights & CASTLE_BQ)
      features[40961] = 1.0f;
    if (pos->castlingRights & CASTLE_WK)
      features[40962] = 1.0f;
    if (pos->castlingRights & CASTLE_WQ)
      features[40963] = 1.0f;
  }

  // Add en-passant file feature (stm perspective-relative)
  if (pos->enPassantSquare != SQ_NONE) {
    features[40964 + FILE_OF(pos->enPassantSquare)] = 1.0f;
  }
}

static inline void accum_add_feature(int32_t *accum, int idx,
                                     const int16_t *weights, int hidden_size) {
  if (idx < 0)
    return;
  const int16_t *w_col = &weights[idx * hidden_size];
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (int i = 0; i < hidden_size; i += 16) {
    int16x8_t w0 = vld1q_s16(&w_col[i]);
    int16x8_t w1 = vld1q_s16(&w_col[i + 8]);

    int32x4_t a0 = vld1q_s32(&accum[i]);
    int32x4_t a1 = vld1q_s32(&accum[i + 4]);
    int32x4_t a2 = vld1q_s32(&accum[i + 8]);
    int32x4_t a3 = vld1q_s32(&accum[i + 12]);

    a0 = vaddw_s16(a0, vget_low_s16(w0));
    a1 = vaddw_high_s16(a1, w0);
    a2 = vaddw_s16(a2, vget_low_s16(w1));
    a3 = vaddw_high_s16(a3, w1);

    vst1q_s32(&accum[i], a0);
    vst1q_s32(&accum[i + 4], a1);
    vst1q_s32(&accum[i + 8], a2);
    vst1q_s32(&accum[i + 12], a3);
  }
#else
  for (int i = 0; i < hidden_size; i++) {
    accum[i] += w_col[i];
  }
#endif
}

static inline void accum_sub_feature(int32_t *accum, int idx,
                                     const int16_t *weights, int hidden_size) {
  if (idx < 0)
    return;
  const int16_t *w_col = &weights[idx * hidden_size];
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (int i = 0; i < hidden_size; i += 16) {
    int16x8_t w0 = vld1q_s16(&w_col[i]);
    int16x8_t w1 = vld1q_s16(&w_col[i + 8]);

    int32x4_t a0 = vld1q_s32(&accum[i]);
    int32x4_t a1 = vld1q_s32(&accum[i + 4]);
    int32x4_t a2 = vld1q_s32(&accum[i + 8]);
    int32x4_t a3 = vld1q_s32(&accum[i + 12]);

    a0 = vsubw_s16(a0, vget_low_s16(w0));
    a1 = vsubw_high_s16(a1, w0);
    a2 = vsubw_s16(a2, vget_low_s16(w1));
    a3 = vsubw_high_s16(a3, w1);

    vst1q_s32(&accum[i], a0);
    vst1q_s32(&accum[i + 4], a1);
    vst1q_s32(&accum[i + 8], a2);
    vst1q_s32(&accum[i + 12], a3);
  }
#else
  for (int i = 0; i < hidden_size; i++) {
    accum[i] -= w_col[i];
  }
#endif
}

static void nnue_refresh_accumulator_perspective(NeuralNetwork *nn,
                                                 Position *pos,
                                                 Color perspective) {
  int hidden_size = nn->sizes[1];
  const int32_t *bias = nn->quant_biases[1];
  const int16_t *weights = nn->quant_weights[1];
  int acc_idx = COLOR_IDX(perspective);
  int ksq = pos->kingSq[acc_idx];

  for (int i = 0; i < hidden_size; i++) {
    pos->accum[acc_idx][i] = bias[i];
  }

  for (int sq = 0; sq < 64; sq++) {
    Piece p = pos->board[sq];
    if (p == EMPTY)
      continue;

    int idx = nnue_feature_index(perspective, p, sq, ksq);
    if (idx >= 0) {
      accum_add_feature(&pos->accum[acc_idx][0], idx, weights, hidden_size);
    }
  }

  if (perspective == WHITE) {
    if (pos->castlingRights & CASTLE_WK)
      accum_add_feature(&pos->accum[0][0], 40960, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WQ)
      accum_add_feature(&pos->accum[0][0], 40961, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BK)
      accum_add_feature(&pos->accum[0][0], 40962, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BQ)
      accum_add_feature(&pos->accum[0][0], 40963, weights, hidden_size);
    if (pos->enPassantSquare != SQ_NONE)
      accum_add_feature(&pos->accum[0][0],
                        40964 + FILE_OF(pos->enPassantSquare), weights,
                        hidden_size);
  } else {
    if (pos->castlingRights & CASTLE_BK)
      accum_add_feature(&pos->accum[1][0], 40960, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BQ)
      accum_add_feature(&pos->accum[1][0], 40961, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WK)
      accum_add_feature(&pos->accum[1][0], 40962, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WQ)
      accum_add_feature(&pos->accum[1][0], 40963, weights, hidden_size);
    if (pos->enPassantSquare != SQ_NONE)
      accum_add_feature(&pos->accum[1][0],
                        40964 + FILE_OF(pos->enPassantSquare), weights,
                        hidden_size);
  }
}

void nnue_refresh_accumulator(NeuralNetwork *nn, Position *pos) {
  if (!nn || !pos)
    return;
  nnue_refresh_accumulator_perspective(nn, pos, WHITE);
  nnue_refresh_accumulator_perspective(nn, pos, BLACK);
}

typedef struct {
  Piece piece;
  int sq;
} AccumulatorChange;

void nnue_update_accumulator(NeuralNetwork *nn, Position *pos, Move m,
                             const struct Undo *u) {
  if (!nn || !pos || !u)
    return;

  int from = MOVE_FROM(m);
  int to = MOVE_TO(m);
  int flag = MOVE_FLAG(m);
  int promo = MOVE_PROMO(m);
  Color us =
      OPPOSITE(pos->sideToMove); // sideToMove was already flipped in apply_move

  Piece moving_piece = u->moving;
  PieceType pt = PIECE_TYPE(moving_piece);

  const int16_t *weights = nn->quant_weights[1];
  int hidden_size = nn->sizes[1];

  int w_ksq = pos->kingSq[COLOR_IDX(WHITE)];
  int b_ksq = pos->kingSq[COLOR_IDX(BLACK)];

  if (pt == KING && us == WHITE) {
    nnue_refresh_accumulator_perspective(nn, pos, WHITE);
  }
  if (pt == KING && us == BLACK) {
    nnue_refresh_accumulator_perspective(nn, pos, BLACK);
  }

  AccumulatorChange removals[3];
  int num_removals = 0;

  AccumulatorChange additions[2];
  int num_additions = 0;

  removals[num_removals++] = (AccumulatorChange){moving_piece, from};

  int is_promo = (pt == PAWN) && (RANK_OF(to) == 0 || RANK_OF(to) == 7);
  if (is_promo) {
    static const PieceType promo_table[] = {KNIGHT, BISHOP, ROOK, QUEEN};
    PieceType promo_pt = (promo <= 3) ? promo_table[promo] : QUEEN;
    Piece promo_piece = MAKE_PIECE(us, promo_pt);
    additions[num_additions++] = (AccumulatorChange){promo_piece, to};
  } else {
    additions[num_additions++] = (AccumulatorChange){moving_piece, to};
  }

  if (u->captured != EMPTY) {
    removals[num_removals++] = (AccumulatorChange){u->captured, u->cap_sq};
  }

  if (flag == MOVE_CASTLE_KS) {
    int r_from = (us == WHITE) ? H1 : H8;
    int r_to = (us == WHITE) ? F1 : F8;
    removals[num_removals++] =
        (AccumulatorChange){MAKE_PIECE(us, ROOK), r_from};
    additions[num_additions++] =
        (AccumulatorChange){MAKE_PIECE(us, ROOK), r_to};
  } else if (flag == MOVE_CASTLE_QS) {
    int r_from = (us == WHITE) ? A1 : A8;
    int r_to = (us == WHITE) ? D1 : D8;
    removals[num_removals++] =
        (AccumulatorChange){MAKE_PIECE(us, ROOK), r_from};
    additions[num_additions++] =
        (AccumulatorChange){MAKE_PIECE(us, ROOK), r_to};
  }

  if (!(pt == KING && us == WHITE)) {
    for (int r = 0; r < num_removals; r++) {
      int idx =
          nnue_feature_index(WHITE, removals[r].piece, removals[r].sq, w_ksq);
      if (idx >= 0)
        accum_sub_feature(&pos->accum[0][0], idx, weights, hidden_size);
    }
    for (int a = 0; a < num_additions; a++) {
      int idx =
          nnue_feature_index(WHITE, additions[a].piece, additions[a].sq, w_ksq);
      if (idx >= 0)
        accum_add_feature(&pos->accum[0][0], idx, weights, hidden_size);
    }
    if (u->old_castling & CASTLE_WK)
      accum_sub_feature(&pos->accum[0][0], 40960, weights, hidden_size);
    if (u->old_castling & CASTLE_WQ)
      accum_sub_feature(&pos->accum[0][0], 40961, weights, hidden_size);
    if (u->old_castling & CASTLE_BK)
      accum_sub_feature(&pos->accum[0][0], 40962, weights, hidden_size);
    if (u->old_castling & CASTLE_BQ)
      accum_sub_feature(&pos->accum[0][0], 40963, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WK)
      accum_add_feature(&pos->accum[0][0], 40960, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WQ)
      accum_add_feature(&pos->accum[0][0], 40961, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BK)
      accum_add_feature(&pos->accum[0][0], 40962, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BQ)
      accum_add_feature(&pos->accum[0][0], 40963, weights, hidden_size);
    if (u->old_ep != SQ_NONE)
      accum_sub_feature(&pos->accum[0][0], 40964 + FILE_OF(u->old_ep), weights,
                        hidden_size);
    if (pos->enPassantSquare != SQ_NONE)
      accum_add_feature(&pos->accum[0][0],
                        40964 + FILE_OF(pos->enPassantSquare), weights,
                        hidden_size);
  }

  if (!(pt == KING && us == BLACK)) {
    for (int r = 0; r < num_removals; r++) {
      int idx =
          nnue_feature_index(BLACK, removals[r].piece, removals[r].sq, b_ksq);
      if (idx >= 0)
        accum_sub_feature(&pos->accum[1][0], idx, weights, hidden_size);
    }
    for (int a = 0; a < num_additions; a++) {
      int idx =
          nnue_feature_index(BLACK, additions[a].piece, additions[a].sq, b_ksq);
      if (idx >= 0)
        accum_add_feature(&pos->accum[1][0], idx, weights, hidden_size);
    }
    if (u->old_castling & CASTLE_BK)
      accum_sub_feature(&pos->accum[1][0], 40960, weights, hidden_size);
    if (u->old_castling & CASTLE_BQ)
      accum_sub_feature(&pos->accum[1][0], 40961, weights, hidden_size);
    if (u->old_castling & CASTLE_WK)
      accum_sub_feature(&pos->accum[1][0], 40962, weights, hidden_size);
    if (u->old_castling & CASTLE_WQ)
      accum_sub_feature(&pos->accum[1][0], 40963, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BK)
      accum_add_feature(&pos->accum[1][0], 40960, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BQ)
      accum_add_feature(&pos->accum[1][0], 40961, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WK)
      accum_add_feature(&pos->accum[1][0], 40962, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WQ)
      accum_add_feature(&pos->accum[1][0], 40963, weights, hidden_size);
    if (u->old_ep != SQ_NONE)
      accum_sub_feature(&pos->accum[1][0], 40964 + FILE_OF(u->old_ep), weights,
                        hidden_size);
    if (pos->enPassantSquare != SQ_NONE)
      accum_add_feature(&pos->accum[1][0],
                        40964 + FILE_OF(pos->enPassantSquare), weights,
                        hidden_size);
  }
}

void nnue_undo_accumulator(NeuralNetwork *nn, Position *pos, Move m,
                           const struct Undo *u) {
  if (!nn || !pos || !u)
    return;

  int from = MOVE_FROM(m);
  int to = MOVE_TO(m);
  int flag = MOVE_FLAG(m);
  int promo = MOVE_PROMO(m);
  Color us = OPPOSITE(pos->sideToMove);

  Piece moving_piece = u->moving;
  PieceType pt = PIECE_TYPE(moving_piece);

  const int16_t *weights = nn->quant_weights[1];
  int hidden_size = nn->sizes[1];

  int w_ksq = pos->kingSq[COLOR_IDX(WHITE)];
  int b_ksq = pos->kingSq[COLOR_IDX(BLACK)];

  if (pt == KING && us == WHITE) {
    nnue_refresh_accumulator_perspective(nn, pos, WHITE);
  }
  if (pt == KING && us == BLACK) {
    nnue_refresh_accumulator_perspective(nn, pos, BLACK);
  }

  AccumulatorChange removals[3];
  int num_removals = 0;

  AccumulatorChange additions[2];
  int num_additions = 0;

  removals[num_removals++] = (AccumulatorChange){moving_piece, from};

  int is_promo = (pt == PAWN) && (RANK_OF(to) == 0 || RANK_OF(to) == 7);
  if (is_promo) {
    static const PieceType promo_table[] = {KNIGHT, BISHOP, ROOK, QUEEN};
    PieceType promo_pt = (promo <= 3) ? promo_table[promo] : QUEEN;
    Piece promo_piece = MAKE_PIECE(us, promo_pt);
    additions[num_additions++] = (AccumulatorChange){promo_piece, to};
  } else {
    additions[num_additions++] = (AccumulatorChange){moving_piece, to};
  }

  if (u->captured != EMPTY) {
    removals[num_removals++] = (AccumulatorChange){u->captured, u->cap_sq};
  }

  if (flag == MOVE_CASTLE_KS) {
    int r_from = (us == WHITE) ? H1 : H8;
    int r_to = (us == WHITE) ? F1 : F8;
    removals[num_removals++] =
        (AccumulatorChange){MAKE_PIECE(us, ROOK), r_from};
    additions[num_additions++] =
        (AccumulatorChange){MAKE_PIECE(us, ROOK), r_to};
  } else if (flag == MOVE_CASTLE_QS) {
    int r_from = (us == WHITE) ? A1 : A8;
    int r_to = (us == WHITE) ? D1 : D8;
    removals[num_removals++] =
        (AccumulatorChange){MAKE_PIECE(us, ROOK), r_from};
    additions[num_additions++] =
        (AccumulatorChange){MAKE_PIECE(us, ROOK), r_to};
  }

  if (!(pt == KING && us == WHITE)) {
    for (int r = 0; r < num_removals; r++) {
      int idx =
          nnue_feature_index(WHITE, removals[r].piece, removals[r].sq, w_ksq);
      if (idx >= 0)
        accum_add_feature(&pos->accum[0][0], idx, weights, hidden_size);
    }
    for (int a = 0; a < num_additions; a++) {
      int idx =
          nnue_feature_index(WHITE, additions[a].piece, additions[a].sq, w_ksq);
      if (idx >= 0)
        accum_sub_feature(&pos->accum[0][0], idx, weights, hidden_size);
    }
    if (pos->castlingRights & CASTLE_WK)
      accum_sub_feature(&pos->accum[0][0], 40960, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WQ)
      accum_sub_feature(&pos->accum[0][0], 40961, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BK)
      accum_sub_feature(&pos->accum[0][0], 40962, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BQ)
      accum_sub_feature(&pos->accum[0][0], 40963, weights, hidden_size);
    if (u->old_castling & CASTLE_WK)
      accum_add_feature(&pos->accum[0][0], 40960, weights, hidden_size);
    if (u->old_castling & CASTLE_WQ)
      accum_add_feature(&pos->accum[0][0], 40961, weights, hidden_size);
    if (u->old_castling & CASTLE_BK)
      accum_add_feature(&pos->accum[0][0], 40962, weights, hidden_size);
    if (u->old_castling & CASTLE_BQ)
      accum_add_feature(&pos->accum[0][0], 40963, weights, hidden_size);
    if (pos->enPassantSquare != SQ_NONE)
      accum_sub_feature(&pos->accum[0][0],
                        40964 + FILE_OF(pos->enPassantSquare), weights,
                        hidden_size);
    if (u->old_ep != SQ_NONE)
      accum_add_feature(&pos->accum[0][0], 40964 + FILE_OF(u->old_ep), weights,
                        hidden_size);
  }

  if (!(pt == KING && us == BLACK)) {
    for (int r = 0; r < num_removals; r++) {
      int idx =
          nnue_feature_index(BLACK, removals[r].piece, removals[r].sq, b_ksq);
      if (idx >= 0)
        accum_add_feature(&pos->accum[1][0], idx, weights, hidden_size);
    }
    for (int a = 0; a < num_additions; a++) {
      int idx =
          nnue_feature_index(BLACK, additions[a].piece, additions[a].sq, b_ksq);
      if (idx >= 0)
        accum_sub_feature(&pos->accum[1][0], idx, weights, hidden_size);
    }
    if (pos->castlingRights & CASTLE_BK)
      accum_sub_feature(&pos->accum[1][0], 40960, weights, hidden_size);
    if (pos->castlingRights & CASTLE_BQ)
      accum_sub_feature(&pos->accum[1][0], 40961, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WK)
      accum_sub_feature(&pos->accum[1][0], 40962, weights, hidden_size);
    if (pos->castlingRights & CASTLE_WQ)
      accum_sub_feature(&pos->accum[1][0], 40963, weights, hidden_size);
    if (u->old_castling & CASTLE_BK)
      accum_add_feature(&pos->accum[1][0], 40960, weights, hidden_size);
    if (u->old_castling & CASTLE_BQ)
      accum_add_feature(&pos->accum[1][0], 40961, weights, hidden_size);
    if (u->old_castling & CASTLE_WK)
      accum_add_feature(&pos->accum[1][0], 40962, weights, hidden_size);
    if (u->old_castling & CASTLE_WQ)
      accum_add_feature(&pos->accum[1][0], 40963, weights, hidden_size);
    if (pos->enPassantSquare != SQ_NONE)
      accum_sub_feature(&pos->accum[1][0],
                        40964 + FILE_OF(pos->enPassantSquare), weights,
                        hidden_size);
    if (u->old_ep != SQ_NONE)
      accum_add_feature(&pos->accum[1][0], 40964 + FILE_OF(u->old_ep), weights,
                        hidden_size);
  }
}

int32_t nnue_evaluate_accumulator(NeuralNetwork *nn, const Position *pos) {
  if (!nn || !pos)
    return 0;

  int stm_idx = (pos->sideToMove == WHITE) ? 0 : 1;
  int opp_idx = 1 - stm_idx;

  int h1_size = nn->sizes[1];
  int concat_size = nn->l2_input_size; // 512 = 256 STM + 256 OPP

  // Thread-safe stack-allocated activations to prevent concurrent write
  // corruption
  int32_t act_scratch[2048];
  int32_t *activations[16];

  if (nn->num_layers > 16 ||
      (concat_size + nn->total_post_input_neurons) > 2048) {
    return 0;
  }

  // Layer 1 activation is concat_size (512), subsequent layers use sizes[l]
  int current_scratch_offset = 0;
  activations[1] = &act_scratch[current_scratch_offset];
  current_scratch_offset += concat_size;
  for (int l = 2; l < nn->num_layers; l++) {
    activations[l] = &act_scratch[current_scratch_offset];
    current_scratch_offset += nn->sizes[l];
  }

  int32_t *act1 = activations[1];

  // CReLU clamp STM accumulator → act1[0..255]
  const int32_t *stm_accum = pos->accum[stm_idx];
  int i = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  int32x4_t zero_vec_s32 = vdupq_n_s32(0);
  int32x4_t max_vec_s32 = vdupq_n_s32(127);
  for (; i <= h1_size - 8; i += 8) {
    int32x4_t acc_lo = vld1q_s32(&stm_accum[i]);
    int32x4_t acc_hi = vld1q_s32(&stm_accum[i + 4]);
    int32x4_t clamped_lo =
        vminq_s32(vmaxq_s32(acc_lo, zero_vec_s32), max_vec_s32);
    int32x4_t clamped_hi =
        vminq_s32(vmaxq_s32(acc_hi, zero_vec_s32), max_vec_s32);
    vst1q_s32(&act1[i], clamped_lo);
    vst1q_s32(&act1[i + 4], clamped_hi);
  }
#endif
  for (; i < h1_size; i++) {
    int32_t val = (stm_accum[i] > 0) ? stm_accum[i] : 0;
    act1[i] = (val < 127) ? val : 127;
  }

  // CReLU clamp opponent accumulator → act1[256..511]
  const int32_t *opp_accum = pos->accum[opp_idx];
  i = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (; i <= h1_size - 8; i += 8) {
    int32x4_t acc_lo = vld1q_s32(&opp_accum[i]);
    int32x4_t acc_hi = vld1q_s32(&opp_accum[i + 4]);
    int32x4_t clamped_lo =
        vminq_s32(vmaxq_s32(acc_lo, zero_vec_s32), max_vec_s32);
    int32x4_t clamped_hi =
        vminq_s32(vmaxq_s32(acc_hi, zero_vec_s32), max_vec_s32);
    vst1q_s32(&act1[h1_size + i], clamped_lo);
    vst1q_s32(&act1[h1_size + i + 4], clamped_hi);
  }
#endif
  for (; i < h1_size; i++) {
    int32_t val = (opp_accum[i] > 0) ? opp_accum[i] : 0;
    act1[h1_size + i] = (val < 127) ? val : 127;
  }

  for (int l = 2; l < nn->num_layers; l++) {
    int n_in = (l == 2) ? concat_size : nn->sizes[l - 1];
    int n_out = nn->sizes[l];

    int32_t *__restrict__ act = activations[l];
    const int32_t *__restrict__ prev_act = activations[l - 1];
    const int32_t *__restrict__ b = nn->quant_biases[l];
    const int16_t *__restrict__ w = nn->quant_weights[l];

    for (int i = 0; i < n_out; i++) {
      const int16_t *__restrict__ w_row = &w[i * n_in];
      int j = 0;
      int32_t sum = b[i];
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
      int32x4_t sum_vec0 = vdupq_n_s32(0);
      int32x4_t sum_vec1 = vdupq_n_s32(0);
      for (; j <= n_in - 8; j += 8) {
        int16x8_t w_val = vld1q_s16(&w_row[j]);
        int32x4_t w0 = vmovl_s16(vget_low_s16(w_val));
        int32x4_t w1 = vmovl_high_s16(w_val);
        int32x4_t act0 = vld1q_s32(&prev_act[j]);
        int32x4_t act1 = vld1q_s32(&prev_act[j + 4]);
        sum_vec0 = vmlaq_s32(sum_vec0, w0, act0);
        sum_vec1 = vmlaq_s32(sum_vec1, w1, act1);
      }
      for (; j <= n_in - 4; j += 4) {
        int16x4_t w_val = vld1_s16(&w_row[j]);
        int32x4_t w_promoted = vmovl_s16(w_val);
        int32x4_t act_val = vld1q_s32(&prev_act[j]);
        sum_vec0 = vmlaq_s32(sum_vec0, w_promoted, act_val);
      }
      sum += vaddvq_s32(vaddq_s32(sum_vec0, sum_vec1));
#endif
      for (; j < n_in; j++) {
        sum += w_row[j] * prev_act[j];
      }

      if (l < nn->num_layers - 1) {
        int32_t val = (sum > 0) ? (sum >> 6) : 0;
        act[i] = (val < 127) ? val : 127;
      } else {
        act[i] = sum;
      }
    }
  }

  return activations[nn->num_layers - 1][0];
}
