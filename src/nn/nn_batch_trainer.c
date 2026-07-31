#include "nn.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

// Adam hyperparameters
#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8f

static int nn_adam_init_if_needed(NeuralNetwork *nn) {
  if (nn->adam_initialized) return 1;
  nn->m_weight_buffer = (float *)calloc(nn->total_weights, sizeof(float));
  nn->m_bias_buffer = (float *)calloc(nn->total_biases, sizeof(float));
  nn->v_weight_buffer = (float *)calloc(nn->total_weights, sizeof(float));
  nn->v_bias_buffer = (float *)calloc(nn->total_biases, sizeof(float));

  if (!nn->m_weight_buffer || !nn->m_bias_buffer || !nn->v_weight_buffer ||
      !nn->v_bias_buffer) {
    free(nn->m_weight_buffer);
    nn->m_weight_buffer = NULL;
    free(nn->m_bias_buffer);
    nn->m_bias_buffer = NULL;
    free(nn->v_weight_buffer);
    nn->v_weight_buffer = NULL;
    free(nn->v_bias_buffer);
    nn->v_bias_buffer = NULL;
    return 0;
  }

  nn->train_step_count = 0;
  nn->adam_initialized = 1;
  return 1;
}

static int extract_features_for_perspective_compact(const CompactPosition *pos,
                                                    Color perspective,
                                                    int *active_indices) {
  if (!pos || !active_indices)
    return 0;
  int count = 0;
  int ksq = pos->kingSq[COLOR_IDX(perspective)];

  for (int sq = 0; sq < 64; sq++) {
    Piece p = pos->board[sq];
    if (p == EMPTY)
      continue;

    int feature_idx = nnue_feature_index(perspective, p, sq, ksq);
    if (feature_idx >= 0 && feature_idx < 40960) {
      active_indices[count++] = feature_idx;
    }
  }

  // Add castling features (perspective-relative)
  if (perspective == WHITE) {
    if (pos->castlingRights & CASTLE_WK)
      active_indices[count++] = 40960;
    if (pos->castlingRights & CASTLE_WQ)
      active_indices[count++] = 40961;
    if (pos->castlingRights & CASTLE_BK)
      active_indices[count++] = 40962;
    if (pos->castlingRights & CASTLE_BQ)
      active_indices[count++] = 40963;
  } else {
    if (pos->castlingRights & CASTLE_BK)
      active_indices[count++] = 40960;
    if (pos->castlingRights & CASTLE_BQ)
      active_indices[count++] = 40961;
    if (pos->castlingRights & CASTLE_WK)
      active_indices[count++] = 40962;
    if (pos->castlingRights & CASTLE_WQ)
      active_indices[count++] = 40963;
  }

  // Add en-passant file feature (perspective-relative)
  if (pos->enPassantSquare != SQ_NONE) {
    active_indices[count++] = 40964 + FILE_OF(pos->enPassantSquare);
  }
  return count;
}

void nn_extract_active_features_dual_compact(const CompactPosition *pos,
                                             int *stm_indices, int *stm_count,
                                             int *opp_indices, int *opp_count) {
  if (!pos)
    return;
  Color stm = pos->sideToMove;
  Color opp = OPPOSITE(stm);
  if (stm_indices && stm_count) {
    *stm_count =
        extract_features_for_perspective_compact(pos, stm, stm_indices);
  }
  if (opp_indices && opp_count) {
    *opp_count =
        extract_features_for_perspective_compact(pos, opp, opp_indices);
  }
}

typedef struct {
  float *activation_buffer;
  float *pre_activation_buffer;
  float *delta_buffer;
  float **activations;
  float **pre_activations;
  float **deltas;

  float *grad_weight_buffer;
  float *grad_bias_buffer;
  float **grad_weights;
  float **grad_biases;

  float loss_sum;
  float quant_loss_sum;
} NNThreadContext;

struct NNBatchTrainer {
  int num_threads;
  NNThreadContext *threads;
};

NNBatchTrainer *nn_batch_trainer_init(NeuralNetwork *nn, int num_threads) {
  if (!nn || num_threads <= 0)
    return NULL;
  if (num_threads > 16)
    num_threads = 16;

  NNBatchTrainer *trainer = (NNBatchTrainer *)calloc(1, sizeof(NNBatchTrainer));
  if (!trainer)
    return NULL;

  trainer->num_threads = num_threads;
  trainer->threads =
      (NNThreadContext *)calloc(num_threads, sizeof(NNThreadContext));
  if (!trainer->threads) {
    free(trainer);
    return NULL;
  }

  int extra = (nn->l2_input_size > nn->sizes[1])
                  ? (nn->l2_input_size - nn->sizes[1])
                  : 0;
  int act_buf_size = nn->total_neurons + extra;
  int post_buf_size = nn->total_post_input_neurons + extra;

  for (int t = 0; t < num_threads; t++) {
    NNThreadContext *ctx = &trainer->threads[t];
    ctx->activation_buffer = (float *)calloc(act_buf_size, sizeof(float));
    ctx->pre_activation_buffer = (float *)calloc(post_buf_size, sizeof(float));
    ctx->delta_buffer = (float *)calloc(post_buf_size, sizeof(float));

    ctx->grad_weight_buffer = (float *)calloc(nn->total_weights, sizeof(float));
    ctx->grad_bias_buffer = (float *)calloc(nn->total_biases, sizeof(float));

    ctx->activations = (float **)malloc(nn->num_layers * sizeof(float *));
    ctx->pre_activations = (float **)malloc(nn->num_layers * sizeof(float *));
    ctx->deltas = (float **)malloc(nn->num_layers * sizeof(float *));
    ctx->grad_weights = (float **)malloc(nn->num_layers * sizeof(float *));
    ctx->grad_biases = (float **)malloc(nn->num_layers * sizeof(float *));

    if (!ctx->activation_buffer || !ctx->pre_activation_buffer ||
        !ctx->delta_buffer || !ctx->grad_weight_buffer ||
        !ctx->grad_bias_buffer || !ctx->activations || !ctx->pre_activations ||
        !ctx->deltas || !ctx->grad_weights || !ctx->grad_biases) {
      nn_batch_trainer_free(trainer);
      return NULL;
    }

    // Partition layer-wise pointers
    float *a_ptr = ctx->activation_buffer;
    float *z_ptr = ctx->pre_activation_buffer;
    float *d_ptr = ctx->delta_buffer;
    float *gw_ptr = ctx->grad_weight_buffer;
    float *gb_ptr = ctx->grad_bias_buffer;

    ctx->activations[0] = a_ptr;
    a_ptr += nn->sizes[0];

    for (int l = 1; l < nn->num_layers; l++) {
      int n_neurons = (l == 1 && nn->l2_input_size > nn->sizes[1])
                          ? nn->l2_input_size
                          : nn->sizes[l];
      int w_cols = (l == 2 && nn->l2_input_size > nn->sizes[1])
                       ? nn->l2_input_size
                       : nn->sizes[l - 1];

      ctx->activations[l] = a_ptr;
      a_ptr += n_neurons;

      ctx->pre_activations[l] = z_ptr;
      z_ptr += n_neurons;

      ctx->deltas[l] = d_ptr;
      d_ptr += n_neurons;

      ctx->grad_weights[l] = gw_ptr;
      gw_ptr += nn->sizes[l] * w_cols;

      ctx->grad_biases[l] = gb_ptr;
      gb_ptr += nn->sizes[l];
    }
  }

  return trainer;
}

void nn_batch_trainer_free(NNBatchTrainer *trainer) {
  if (!trainer)
    return;
  if (trainer->threads) {
    for (int t = 0; t < trainer->num_threads; t++) {
      NNThreadContext *ctx = &trainer->threads[t];
      free(ctx->activation_buffer);
      free(ctx->pre_activation_buffer);
      free(ctx->delta_buffer);
      free(ctx->grad_weight_buffer);
      free(ctx->grad_bias_buffer);
      free(ctx->activations);
      free(ctx->pre_activations);
      free(ctx->deltas);
      free(ctx->grad_weights);
      free(ctx->grad_biases);
    }
    free(trainer->threads);
  }
  free(trainer);
}

typedef struct {
  NNThreadContext *thread_ctx;
  NeuralNetwork *nn;
  const TrainingSample *samples;
  int start_idx;
  int end_idx;
} TrainWorkerArgs;

static void *train_worker_routine(void *arg) {
  TrainWorkerArgs *args = (TrainWorkerArgs *)arg;
  NNThreadContext *ctx = args->thread_ctx;
  NeuralNetwork *nn = args->nn;
  const TrainingSample *samples = args->samples;

  memset(ctx->grad_weight_buffer, 0, nn->total_weights * sizeof(float));
  memset(ctx->grad_bias_buffer, 0, nn->total_biases * sizeof(float));
  ctx->loss_sum = 0.0f;

  int stm_indices[64], opp_indices[64];

  for (int idx = args->start_idx; idx < args->end_idx; idx++) {
    const CompactPosition *pos = &samples[idx].pos;
    float target = samples[idx].target;

    int stm_count = 0, opp_count = 0;
    nn_extract_active_features_dual_compact(pos, stm_indices, &stm_count,
                                            opp_indices, &opp_count);

    // --- Layer 1 Forward (Dual perspective) ---
    int h1_size = nn->sizes[1]; // 256
    float *__restrict__ z1 = ctx->pre_activations[1];
    float *__restrict__ a1 = ctx->activations[1];
    const float *__restrict__ b1 = nn->biases[1];
    const float *__restrict__ w1 = nn->weights[1];
    int in_size = nn->sizes[0];

    // 1) Side-to-move perspective -> a1[0..255]
    for (int i = 0; i < h1_size; i++) {
      float sum = b1[i];
      const float *w_row = &w1[i * in_size];
      for (int k = 0; k < stm_count; k++) {
        sum += w_row[stm_indices[k]];
      }
      z1[i] = sum;
      float val = (sum > 0.0f) ? sum : 0.0f;
      a1[i] = (val < 1.0f) ? val : 1.0f;
    }

    // 2) Opponent perspective -> a1[256..511]
    for (int i = 0; i < h1_size; i++) {
      float sum = b1[i];
      const float *w_row = &w1[i * in_size];
      for (int k = 0; k < opp_count; k++) {
        sum += w_row[opp_indices[k]];
      }
      z1[h1_size + i] = sum;
      float val = (sum > 0.0f) ? sum : 0.0f;
      a1[h1_size + i] = (val < 1.0f) ? val : 1.0f;
    }

    // --- Subsequent Layers Forward ---
    for (int l = 2; l < nn->num_layers; l++) {
      int n_in = (l == 2 && nn->l2_input_size > nn->sizes[1])
                     ? nn->l2_input_size
                     : nn->sizes[l - 1];
      int n_out = nn->sizes[l];

      float *__restrict__ act = ctx->activations[l];
      float *__restrict__ pre_act = ctx->pre_activations[l];
      const float *__restrict__ prev_act = ctx->activations[l - 1];
      const float *__restrict__ b = nn->biases[l];
      const float *__restrict__ w = nn->weights[l];

      for (int i = 0; i < n_out; i++) {
        float sum = b[i];
        const float *w_row = &w[i * n_in];
        for (int j = 0; j < n_in; j++) {
          sum += w_row[j] * prev_act[j];
        }
        pre_act[i] = sum;
        if (l < nn->num_layers - 1) {
          float val = (sum > 0.0f) ? sum : 0.0f;
          act[i] = (val < 1.0f) ? val : 1.0f;
        } else {
          act[i] = 1.0f / (1.0f + expf(-sum));
        }
      }
    }

    // --- Loss & Deltas ---
    int L = nn->num_layers - 1;
    float output = ctx->activations[L][0];
    float error = output - target;
    ctx->loss_sum += 0.5f * error * error;
    ctx->deltas[L][0] = error * output * (1.0f - output);

    // Backpropagate deltas through hidden layers
    for (int l = L - 1; l >= 1; l--) {
      int n_curr = (l == 1 && nn->l2_input_size > nn->sizes[1])
                       ? nn->l2_input_size
                       : nn->sizes[l];
      int n_next = nn->sizes[l + 1];
      int w_cols = n_curr;

      float *__restrict__ d_curr = ctx->deltas[l];
      const float *__restrict__ d_next = ctx->deltas[l + 1];
      const float *__restrict__ w_next = nn->weights[l + 1];
      const float *__restrict__ z_curr = ctx->pre_activations[l];

      memset(d_curr, 0, n_curr * sizeof(float));

      for (int j = 0; j < n_next; j++) {
        float d_val = d_next[j];
        const float *w_row = &w_next[j * w_cols];
        for (int i = 0; i < n_curr; i++) {
          d_curr[i] += d_val * w_row[i];
        }
      }

      for (int i = 0; i < n_curr; i++) {
        if (z_curr[i] <= 0.0f || z_curr[i] >= 1.0f) {
          d_curr[i] = 0.0f;
        }
      }
    }

    // --- Gradient Accumulation ---
    // Layers 2 to L
    for (int l = 2; l < nn->num_layers; l++) {
      int n_in = (l == 2 && nn->l2_input_size > nn->sizes[1])
                     ? nn->l2_input_size
                     : nn->sizes[l - 1];
      int n_out = nn->sizes[l];

      float *__restrict__ gw = ctx->grad_weights[l];
      float *__restrict__ gb = ctx->grad_biases[l];
      const float *__restrict__ d = ctx->deltas[l];
      const float *__restrict__ prev_act = ctx->activations[l - 1];

      for (int i = 0; i < n_out; i++) {
        float d_val = d[i];
        gb[i] += d_val;
        float *gw_row = &gw[i * n_in];
        for (int j = 0; j < n_in; j++) {
          gw_row[j] += d_val * prev_act[j];
        }
      }
    }

    // Layer 1 Sparse Gradient Accumulation (accumulates deltas from both
    // perspectives into shared weights)
    float *__restrict__ gw1 = ctx->grad_weights[1];
    float *__restrict__ gb1 = ctx->grad_biases[1];
    const float *__restrict__ d1 = ctx->deltas[1];

    // 1) STM perspective deltas d1[0..255]
    for (int i = 0; i < h1_size; i++) {
      float d_val = d1[i];
      if (d_val != 0.0f) {
        gb1[i] += d_val;
        float *gw_row = &gw1[i * in_size];
        for (int k = 0; k < stm_count; k++) {
          gw_row[stm_indices[k]] += d_val;
        }
      }
    }

    // 2) Opponent perspective deltas d1[256..511]
    for (int i = 0; i < h1_size; i++) {
      float d_val = d1[h1_size + i];
      if (d_val != 0.0f) {
        gb1[i] += d_val;
        float *gw_row = &gw1[i * in_size];
        for (int k = 0; k < opp_count; k++) {
          gw_row[opp_indices[k]] += d_val;
        }
      }
    }
  }

  return NULL;
}

typedef struct {
  NNBatchTrainer *trainer;
  NeuralNetwork *nn;
  int thread_idx;
  float inv_batch;
  float lr_corrected;
  float bc2_sqrt_inv;
  float weight_decay;
  float learning_rate;
} AdamWorkerArgs;

static void *adam_worker_routine(void *arg) {
  AdamWorkerArgs *args = (AdamWorkerArgs *)arg;
  NNBatchTrainer *trainer = args->trainer;
  NeuralNetwork *nn = args->nn;
  int num_threads = trainer->num_threads;

  int total_weights = nn->total_weights;
  int total_biases = nn->total_biases;

  int w_start = (total_weights * args->thread_idx) / num_threads;
  int w_end = (total_weights * (args->thread_idx + 1)) / num_threads;

  for (int i = w_start; i < w_end; i++) {
    float grad_w = 0.0f;
    for (int t = 0; t < num_threads; t++) {
      grad_w += trainer->threads[t].grad_weight_buffer[i];
    }
    grad_w *= args->inv_batch;

    nn->m_weight_buffer[i] =
        ADAM_BETA1 * nn->m_weight_buffer[i] + (1.0f - ADAM_BETA1) * grad_w;
    nn->v_weight_buffer[i] = ADAM_BETA2 * nn->v_weight_buffer[i] +
                             (1.0f - ADAM_BETA2) * grad_w * grad_w;

    nn->weight_buffer[i] -=
        args->lr_corrected * nn->m_weight_buffer[i] /
        (sqrtf(nn->v_weight_buffer[i]) * args->bc2_sqrt_inv + ADAM_EPSILON);
    nn->weight_buffer[i] -=
        args->learning_rate * args->weight_decay * nn->weight_buffer[i];
  }

  int b_start = (total_biases * args->thread_idx) / num_threads;
  int b_end = (total_biases * (args->thread_idx + 1)) / num_threads;

  for (int i = b_start; i < b_end; i++) {
    float grad_b = 0.0f;
    for (int t = 0; t < num_threads; t++) {
      grad_b += trainer->threads[t].grad_bias_buffer[i];
    }
    grad_b *= args->inv_batch;

    nn->m_bias_buffer[i] =
        ADAM_BETA1 * nn->m_bias_buffer[i] + (1.0f - ADAM_BETA1) * grad_b;
    nn->v_bias_buffer[i] = ADAM_BETA2 * nn->v_bias_buffer[i] +
                           (1.0f - ADAM_BETA2) * grad_b * grad_b;

    nn->bias_buffer[i] -=
        args->lr_corrected * nn->m_bias_buffer[i] /
        (sqrtf(nn->v_bias_buffer[i]) * args->bc2_sqrt_inv + ADAM_EPSILON);
  }

  return NULL;
}

float nn_train_batch_parallel(NNBatchTrainer *trainer, NeuralNetwork *nn,
                              const TrainingSample *samples, int batch_size,
                              float learning_rate, float weight_decay) {
  if (!trainer || !nn || !samples || batch_size <= 0)
    return NAN;

  if (!nn_adam_init_if_needed(nn))
    return NAN;

  int num_threads = trainer->num_threads;
  pthread_t thread_handles[64];
  TrainWorkerArgs train_args[64];

  int samples_per_thread = batch_size / num_threads;
  int extra = batch_size % num_threads;
  int current_idx = 0;

  for (int t = 0; t < num_threads; t++) {
    int count = samples_per_thread + (t < extra ? 1 : 0);
    train_args[t].thread_ctx = &trainer->threads[t];
    train_args[t].nn = nn;
    train_args[t].samples = samples;
    train_args[t].start_idx = current_idx;
    train_args[t].end_idx = current_idx + count;
    current_idx += count;

    pthread_create(&thread_handles[t], NULL, train_worker_routine,
                   &train_args[t]);
  }

  float total_loss = 0.0f;
  for (int t = 0; t < num_threads; t++) {
    pthread_join(thread_handles[t], NULL);
    total_loss += trainer->threads[t].loss_sum;
  }

  // --- AdamW Parameter Update Step ---
  nn->train_step_count++;
  int step_cnt = nn->train_step_count;

  float bc1 = 1.0f - powf(ADAM_BETA1, (float)step_cnt);
  float bc2 = 1.0f - powf(ADAM_BETA2, (float)step_cnt);
  float lr_corrected = learning_rate / bc1;
  float bc2_sqrt_inv = 1.0f / sqrtf(bc2);
  float inv_batch = 1.0f / (float)batch_size;

  AdamWorkerArgs adam_args[64];
  for (int t = 0; t < num_threads; t++) {
    adam_args[t].trainer = trainer;
    adam_args[t].nn = nn;
    adam_args[t].thread_idx = t;
    adam_args[t].inv_batch = inv_batch;
    adam_args[t].lr_corrected = lr_corrected;
    adam_args[t].bc2_sqrt_inv = bc2_sqrt_inv;
    adam_args[t].weight_decay = weight_decay;
    adam_args[t].learning_rate = learning_rate;

    pthread_create(&thread_handles[t], NULL, adam_worker_routine,
                   &adam_args[t]);
  }

  for (int t = 0; t < num_threads; t++) {
    pthread_join(thread_handles[t], NULL);
  }

  return total_loss;
}

typedef struct {
  NNThreadContext *thread_ctx;
  NeuralNetwork *nn;
  const TrainingSample *samples;
  int start_idx;
  int end_idx;
} EvalWorkerArgs;

static void *eval_worker_routine(void *arg) {
  EvalWorkerArgs *args = (EvalWorkerArgs *)arg;
  NNThreadContext *ctx = args->thread_ctx;
  NeuralNetwork *nn = args->nn;
  const TrainingSample *samples = args->samples;

  ctx->loss_sum = 0.0f;
  ctx->quant_loss_sum = 0.0f;
  int stm_indices[64], opp_indices[64];

  for (int idx = args->start_idx; idx < args->end_idx; idx++) {
    const CompactPosition *cpos = &samples[idx].pos;
    float target = samples[idx].target;

    int stm_count = 0, opp_count = 0;
    nn_extract_active_features_dual_compact(cpos, stm_indices, &stm_count,
                                            opp_indices, &opp_count);

    // Float forward pass (Dual perspective)
    int h1_size = nn->sizes[1];
    float *z1 = ctx->pre_activations[1];
    float *a1 = ctx->activations[1];
    const float *b1 = nn->biases[1];
    const float *w1 = nn->weights[1];
    int in_size = nn->sizes[0];

    // 1) STM perspective -> a1[0..255]
    for (int i = 0; i < h1_size; i++) {
      float sum = b1[i];
      const float *w_row = &w1[i * in_size];
      for (int k = 0; k < stm_count; k++) {
        sum += w_row[stm_indices[k]];
      }
      z1[i] = sum;
      float val = (sum > 0.0f) ? sum : 0.0f;
      a1[i] = (val < 1.0f) ? val : 1.0f;
    }

    // 2) Opponent perspective -> a1[256..511]
    for (int i = 0; i < h1_size; i++) {
      float sum = b1[i];
      const float *w_row = &w1[i * in_size];
      for (int k = 0; k < opp_count; k++) {
        sum += w_row[opp_indices[k]];
      }
      z1[h1_size + i] = sum;
      float val = (sum > 0.0f) ? sum : 0.0f;
      a1[h1_size + i] = (val < 1.0f) ? val : 1.0f;
    }

    for (int l = 2; l < nn->num_layers; l++) {
      int n_in = (l == 2 && nn->l2_input_size > nn->sizes[1])
                     ? nn->l2_input_size
                     : nn->sizes[l - 1];
      int n_out = nn->sizes[l];

      float *act = ctx->activations[l];
      float *pre_act = ctx->pre_activations[l];
      const float *prev_act = ctx->activations[l - 1];
      const float *b = nn->biases[l];
      const float *w = nn->weights[l];

      for (int i = 0; i < n_out; i++) {
        float sum = b[i];
        const float *w_row = &w[i * n_in];
        for (int j = 0; j < n_in; j++) {
          sum += w_row[j] * prev_act[j];
        }
        pre_act[i] = sum;
        if (l < nn->num_layers - 1) {
          float val = (sum > 0.0f) ? sum : 0.0f;
          act[i] = (val < 1.0f) ? val : 1.0f;
        } else {
          act[i] = 1.0f / (1.0f + expf(-sum));
        }
      }
    }

    float float_out = ctx->activations[nn->num_layers - 1][0];
    float diff_float = float_out - target;
    ctx->loss_sum += 0.5f * diff_float * diff_float;

    // Quantized evaluation
    Position pos;
    compact_to_position(cpos, &pos);
    nnue_refresh_accumulator(nn, &pos);
    int32_t quant_raw = nnue_evaluate_accumulator(nn, &pos);
    float quant_out = 1.0f / (1.0f + expf(-(float)quant_raw / 8192.0f));
    float diff_quant = quant_out - target;
    ctx->quant_loss_sum += 0.5f * diff_quant * diff_quant;
  }

  return NULL;
}

float nn_evaluate_batch_parallel(NNBatchTrainer *trainer, NeuralNetwork *nn,
                                 const TrainingSample *samples, int num_samples,
                                 float *out_quant_loss) {
  if (!trainer || !nn || !samples || num_samples <= 0) {
    if (out_quant_loss)
      *out_quant_loss = 0.0f;
    return 0.0f;
  }

  nn_quantize(nn);

  int num_threads = trainer->num_threads;
  pthread_t thread_handles[64];
  EvalWorkerArgs eval_args[64];

  int samples_per_thread = num_samples / num_threads;
  int extra = num_samples % num_threads;
  int current_idx = 0;

  for (int t = 0; t < num_threads; t++) {
    int count = samples_per_thread + (t < extra ? 1 : 0);
    eval_args[t].thread_ctx = &trainer->threads[t];
    eval_args[t].nn = nn;
    eval_args[t].samples = samples;
    eval_args[t].start_idx = current_idx;
    eval_args[t].end_idx = current_idx + count;
    current_idx += count;

    pthread_create(&thread_handles[t], NULL, eval_worker_routine,
                   &eval_args[t]);
  }

  float total_float_loss = 0.0f;
  float total_quant_loss = 0.0f;

  for (int t = 0; t < num_threads; t++) {
    pthread_join(thread_handles[t], NULL);
    total_float_loss += trainer->threads[t].loss_sum;
    total_quant_loss += trainer->threads[t].quant_loss_sum;
  }

  if (out_quant_loss) {
    *out_quant_loss = total_quant_loss / num_samples;
  }
  return total_float_loss / num_samples;
}

float nn_train_step_pos(NeuralNetwork *nn, const Position *pos, float target,
                        float learning_rate, float weight_decay) {
  if (!nn || !pos)
    return NAN;
  TrainingSample sample;
  sample.pos = position_to_compact(pos);
  sample.target = target;
  NNBatchTrainer *trainer = nn_batch_trainer_init(nn, 1);
  if (!trainer)
    return NAN;
  float loss = nn_train_batch_parallel(trainer, nn, &sample, 1, learning_rate,
                                       weight_decay);
  nn_batch_trainer_free(trainer);
  return loss;
}
