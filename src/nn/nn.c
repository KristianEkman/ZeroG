#include "nn.h"
#include "movegen.h"
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

NeuralNetwork *nn_init(const int *sizes, int num_layers) {
  if (!sizes || num_layers < 2) {
    return NULL;
  }
  if (sizes[num_layers - 1] != 1) {
    return NULL; // Must have exactly one output neuron
  }

  NeuralNetwork *nn = (NeuralNetwork *)malloc(sizeof(NeuralNetwork));
  if (!nn)
    return NULL;

  nn->num_layers = num_layers;
  nn->sizes = (int *)malloc(num_layers * sizeof(int));
  if (!nn->sizes) {
    free(nn);
    return NULL;
  }
  memcpy(nn->sizes, sizes, num_layers * sizeof(int));

  // Initialize all buffers to NULL to ensure safe cleanup if an allocation
  // fails
  nn->weight_buffer = NULL;
  nn->bias_buffer = NULL;
  nn->activation_buffer = NULL;
  nn->pre_activation_buffer = NULL;
  nn->delta_buffer = NULL;
  nn->weights = NULL;
  nn->biases = NULL;
  nn->activations = NULL;
  nn->pre_activations = NULL;
  nn->deltas = NULL;

  nn->quant_weight_buffer = NULL;
  nn->quant_bias_buffer = NULL;
  nn->quant_activation_buffer = NULL;
  nn->quant_weights = NULL;
  nn->quant_biases = NULL;
  nn->quant_activations = NULL;

  // Adam optimizer state (lazily allocated on first train step)
  nn->m_weight_buffer = NULL;
  nn->m_bias_buffer = NULL;
  nn->v_weight_buffer = NULL;
  nn->v_bias_buffer = NULL;
  nn->adam_initialized = 0;
  nn->train_step_count = 0;
  nn->l2_input_size = 0;

  // Detect NNUE dual-perspective mode: layer 2 input width is 2*accumulator
  // size
  if (num_layers == 4 && sizes[0] == NN_INPUT_SIZE &&
      sizes[1] == NN_ACCUM_SIZE) {
    nn->l2_input_size = NN_L2_INPUT_SIZE;
  } else {
    nn->l2_input_size = (num_layers >= 3) ? sizes[1] : 0;
  }

  // Calculate total buffer sizes needed
  nn->total_weights = 0;
  nn->total_biases = 0;
  nn->total_neurons = 0;
  nn->total_post_input_neurons = 0;

  for (int l = 0; l < num_layers; l++) {
    int act_len =
        (l == 1 && nn->l2_input_size > sizes[1]) ? nn->l2_input_size : sizes[l];
    nn->total_neurons += act_len;
    if (l > 0) {
      int w_cols = (l == 2 && nn->l2_input_size > sizes[1]) ? nn->l2_input_size
                                                            : sizes[l - 1];
      nn->total_weights += sizes[l] * w_cols;
      nn->total_biases += sizes[l];
      nn->total_post_input_neurons += act_len;
    }
  }

  // Allocate memory blocks (use calloc to zero activations, biases, and deltas)
  nn->weight_buffer = (float *)malloc(nn->total_weights * sizeof(float));
  nn->bias_buffer = (float *)calloc(nn->total_biases, sizeof(float));
  nn->activation_buffer = (float *)calloc(nn->total_neurons, sizeof(float));
  nn->pre_activation_buffer =
      (float *)calloc(nn->total_post_input_neurons, sizeof(float));
  nn->delta_buffer =
      (float *)calloc(nn->total_post_input_neurons, sizeof(float));

  // Allocate quantized memory blocks
  nn->quant_weight_buffer =
      (int16_t *)malloc(nn->total_weights * sizeof(int16_t));
  nn->quant_bias_buffer = (int32_t *)calloc(nn->total_biases, sizeof(int32_t));
  nn->quant_activation_buffer =
      (int32_t *)calloc(nn->total_neurons, sizeof(int32_t));

  // Allocate pointer arrays
  nn->weights = (float **)malloc(num_layers * sizeof(float *));
  nn->biases = (float **)malloc(num_layers * sizeof(float *));
  nn->activations = (float **)malloc(num_layers * sizeof(float *));
  nn->pre_activations = (float **)malloc(num_layers * sizeof(float *));
  nn->deltas = (float **)malloc(num_layers * sizeof(float *));

  nn->quant_weights = (int16_t **)malloc(num_layers * sizeof(int16_t *));
  nn->quant_biases = (int32_t **)malloc(num_layers * sizeof(int32_t *));
  nn->quant_activations = (int32_t **)malloc(num_layers * sizeof(int32_t *));

  // Check if any allocations failed
  if (!nn->weight_buffer || !nn->bias_buffer || !nn->activation_buffer ||
      !nn->pre_activation_buffer || !nn->delta_buffer || !nn->weights ||
      !nn->biases || !nn->activations || !nn->pre_activations || !nn->deltas ||
      !nn->quant_weight_buffer || !nn->quant_bias_buffer ||
      !nn->quant_activation_buffer || !nn->quant_weights || !nn->quant_biases ||
      !nn->quant_activations) {
    nn_free(nn);
    return NULL;
  }

  // Partition the contiguous buffers into layer-wise chunks
  float *w_ptr = nn->weight_buffer;
  float *b_ptr = nn->bias_buffer;
  float *a_ptr = nn->activation_buffer;
  float *z_ptr = nn->pre_activation_buffer;
  float *d_ptr = nn->delta_buffer;

  nn->activations[0] = a_ptr;
  a_ptr += sizes[0];

  for (int l = 1; l < num_layers; l++) {
    int w_cols = (l == 2 && nn->l2_input_size > sizes[1]) ? nn->l2_input_size
                                                          : sizes[l - 1];
    int act_len =
        (l == 1 && nn->l2_input_size > sizes[1]) ? nn->l2_input_size : sizes[l];

    nn->weights[l] = w_ptr;
    w_ptr += sizes[l] * w_cols;

    nn->biases[l] = b_ptr;
    b_ptr += sizes[l];

    nn->activations[l] = a_ptr;
    a_ptr += act_len;

    nn->pre_activations[l] = z_ptr;
    z_ptr += act_len;

    nn->deltas[l] = d_ptr;
    d_ptr += act_len;
  }

  // Partition the quantized buffers into layer-wise chunks
  int16_t *qw_ptr = nn->quant_weight_buffer;
  int32_t *qb_ptr = nn->quant_bias_buffer;
  int32_t *qa_ptr = nn->quant_activation_buffer;

  nn->quant_activations[0] = qa_ptr;
  qa_ptr += sizes[0];

  for (int l = 1; l < num_layers; l++) {
    int w_cols = (l == 2 && nn->l2_input_size > sizes[1]) ? nn->l2_input_size
                                                          : sizes[l - 1];
    int act_len =
        (l == 1 && nn->l2_input_size > sizes[1]) ? nn->l2_input_size : sizes[l];

    nn->quant_weights[l] = qw_ptr;
    qw_ptr += sizes[l] * w_cols;

    nn->quant_biases[l] = qb_ptr;
    qb_ptr += sizes[l];

    nn->quant_activations[l] = qa_ptr;
    qa_ptr += act_len;
  }

  // Initialize weights using He / Xavier initialization
  // Use a local deterministic PRNG (Xorshift32) to ensure reproducibility
  uint32_t rng_state = 314159265; // Pi seed
  for (int l = 1; l < num_layers; l++) {
    int n_in = (l == 2 && nn->l2_input_size > sizes[1]) ? nn->l2_input_size
                                                        : sizes[l - 1];
    int n_out = sizes[l];
    float limit;
    if (l < num_layers - 1) {
      // He (Kaiming) initialization for ReLU activations
      limit = sqrtf(6.0f / (float)n_in);
    } else {
      // Xavier (Glorot) initialization for the output layer (linear activation)
      limit = sqrtf(6.0f / (float)(n_in + n_out));
    }

    int size = n_out * n_in;
    for (int i = 0; i < size; i++) {
      rng_state ^= rng_state << 13;
      rng_state ^= rng_state >> 17;
      rng_state ^= rng_state << 5;
      float r = (float)((double)rng_state / 4294967295.0);
      nn->weights[l][i] = -limit + r * (2.0f * limit);
    }

    // Biases are already initialized to 0 by calloc
  }

  nn_quantize(nn);

  return nn;
}

float nn_forward(NeuralNetwork *nn, const float *inputs) {
  if (!nn || !inputs)
    return 0.0f;

  // Copy inputs to the input layer (layer 0 activations)
  memcpy(nn->activations[0], inputs, nn->sizes[0] * sizeof(float));

  // Propagate forward through layers
  for (int l = 1; l < nn->num_layers; l++) {
    int n_in = (l == 2 && nn->l2_input_size > nn->sizes[1]) ? nn->l2_input_size
                                                            : nn->sizes[l - 1];
    int n_out = nn->sizes[l];

    float *__restrict__ act = nn->activations[l];
    float *__restrict__ pre_act = nn->pre_activations[l];
    const float *__restrict__ prev_act = nn->activations[l - 1];
    const float *__restrict__ b = nn->biases[l];
    const float *__restrict__ w = nn->weights[l];

    for (int i = 0; i < n_out; i++) {
      float sum = b[i];
      const float *__restrict__ w_row = &w[i * n_in];

      int j = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
      float32x4_t sum_vec0 = vdupq_n_f32(0.0f);
      float32x4_t sum_vec1 = vdupq_n_f32(0.0f);
      float32x4_t sum_vec2 = vdupq_n_f32(0.0f);
      float32x4_t sum_vec3 = vdupq_n_f32(0.0f);

      for (; j <= n_in - 16; j += 16) {
        float32x4_t w_vec0 = vld1q_f32(&w_row[j]);
        float32x4_t a_vec0 = vld1q_f32(&prev_act[j]);
        sum_vec0 = vmlaq_f32(sum_vec0, w_vec0, a_vec0);

        float32x4_t w_vec1 = vld1q_f32(&w_row[j + 4]);
        float32x4_t a_vec1 = vld1q_f32(&prev_act[j + 4]);
        sum_vec1 = vmlaq_f32(sum_vec1, w_vec1, a_vec1);

        float32x4_t w_vec2 = vld1q_f32(&w_row[j + 8]);
        float32x4_t a_vec2 = vld1q_f32(&prev_act[j + 8]);
        sum_vec2 = vmlaq_f32(sum_vec2, w_vec2, a_vec2);

        float32x4_t w_vec3 = vld1q_f32(&w_row[j + 12]);
        float32x4_t a_vec3 = vld1q_f32(&prev_act[j + 12]);
        sum_vec3 = vmlaq_f32(sum_vec3, w_vec3, a_vec3);
      }
      for (; j <= n_in - 4; j += 4) {
        float32x4_t w_vec = vld1q_f32(&w_row[j]);
        float32x4_t a_vec = vld1q_f32(&prev_act[j]);
        sum_vec0 = vmlaq_f32(sum_vec0, w_vec, a_vec);
      }
      float32x4_t total_vec = vaddq_f32(vaddq_f32(sum_vec0, sum_vec1),
                                        vaddq_f32(sum_vec2, sum_vec3));
      sum += vaddvq_f32(total_vec);
#endif
      for (; j < n_in; j++) {
        sum += w_row[j] * prev_act[j];
      }

      pre_act[i] = sum;

      // Activation functions
      if (l < nn->num_layers - 1) {
        // Clipped ReLU for hidden layers
        float val = (sum > 0.0f) ? sum : 0.0f;
        act[i] = (val < 1.0f) ? val : 1.0f;
      } else {
        // Sigmoid activation for the single output neuron (WDL probability)
        act[i] = 1.0f / (1.0f + expf(-sum));
      }
    }

    if (l == 1 && nn->l2_input_size > nn->sizes[1]) {
      int h1 = nn->sizes[1];
      memcpy(&act[h1], act, h1 * sizeof(float));
      memcpy(&pre_act[h1], pre_act, h1 * sizeof(float));
    }
  }

  // Return the single scalar output from the final layer
  return nn->activations[nn->num_layers - 1][0];
}

// Adam hyperparameters
#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8f

static int nn_adam_init(NeuralNetwork *nn) {
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

float nn_train_step(NeuralNetwork *nn, const float *inputs, float target,
                    float learning_rate, float weight_decay) {
  if (!nn || !inputs)
    return NAN;

  // Lazily allocate Adam state on first training call
  if (!nn->adam_initialized) {
    if (!nn_adam_init(nn))
      return NAN;
  }

  // 1. Forward Propagation
  float output = nn_forward(nn, inputs);

  // Compute Loss (Mean Squared Error: L = 0.5 * (output - target)^2)
  float error = output - target;
  float loss = 0.5f * error * error;

  // 2. Backward Propagation (Compute Deltas)
  int L = nn->num_layers - 1;

  // Output Layer delta: derivative of MSE loss * derivative of Sigmoid output
  // activation (output * (1.0 - output))
  nn->deltas[L][0] = error * output * (1.0f - output);

  // Backpropagate deltas through hidden layers
  for (int l = L - 1; l >= 1; l--) {
    int n_curr = (l == 1 && nn->l2_input_size > nn->sizes[1])
                     ? nn->l2_input_size
                     : nn->sizes[l];
    int n_next = nn->sizes[l + 1];
    int w_cols = n_curr;

    float *__restrict__ d_curr = nn->deltas[l];
    const float *__restrict__ d_next = nn->deltas[l + 1];
    const float *__restrict__ w_next = nn->weights[l + 1];
    const float *__restrict__ z_curr = nn->pre_activations[l];

    // Zero out current layer delta first (swapped loops)
    memset(d_curr, 0, n_curr * sizeof(float));

    for (int j = 0; j < n_next; j++) {
      float d_val = d_next[j];
      const float *__restrict__ w_row = &w_next[j * w_cols];

      int i = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
      float32x4_t d_val_vec = vdupq_n_f32(d_val);
      for (; i <= n_curr - 16; i += 16) {
        float32x4_t d_curr_vec0 = vld1q_f32(&d_curr[i]);
        float32x4_t w_vec0 = vld1q_f32(&w_row[i]);
        d_curr_vec0 = vmlaq_f32(d_curr_vec0, d_val_vec, w_vec0);
        vst1q_f32(&d_curr[i], d_curr_vec0);

        float32x4_t d_curr_vec1 = vld1q_f32(&d_curr[i + 4]);
        float32x4_t w_vec1 = vld1q_f32(&w_row[i + 4]);
        d_curr_vec1 = vmlaq_f32(d_curr_vec1, d_val_vec, w_vec1);
        vst1q_f32(&d_curr[i + 4], d_curr_vec1);

        float32x4_t d_curr_vec2 = vld1q_f32(&d_curr[i + 8]);
        float32x4_t w_vec2 = vld1q_f32(&w_row[i + 8]);
        d_curr_vec2 = vmlaq_f32(d_curr_vec2, d_val_vec, w_vec2);
        vst1q_f32(&d_curr[i + 8], d_curr_vec2);

        float32x4_t d_curr_vec3 = vld1q_f32(&d_curr[i + 12]);
        float32x4_t w_vec3 = vld1q_f32(&w_row[i + 12]);
        d_curr_vec3 = vmlaq_f32(d_curr_vec3, d_val_vec, w_vec3);
        vst1q_f32(&d_curr[i + 12], d_curr_vec3);
      }
      for (; i <= n_curr - 4; i += 4) {
        float32x4_t d_curr_vec = vld1q_f32(&d_curr[i]);
        float32x4_t w_vec = vld1q_f32(&w_row[i]);
        d_curr_vec = vmlaq_f32(d_curr_vec, d_val_vec, w_vec);
        vst1q_f32(&d_curr[i], d_curr_vec);
      }
#endif
      for (; i < n_curr; i++) {
        d_curr[i] += d_val * w_row[i];
      }
    }

    // Apply Clipped ReLU derivative: delta is 0 if z <= 0 or z >= 1.0
    for (int i = 0; i < n_curr; i++) {
      if (z_curr[i] <= 0.0f || z_curr[i] >= 1.0f) {
        d_curr[i] = 0.0f;
      }
    }
  }

  // 3. Update Weights and Biases (AdamW)
  nn->train_step_count++;
  int t = nn->train_step_count;

  // Bias correction terms
  float bc1 = 1.0f - powf(ADAM_BETA1, (float)t);
  float bc2 = 1.0f - powf(ADAM_BETA2, (float)t);
  float lr_corrected = learning_rate / bc1;
  float bc2_sqrt_inv = 1.0f / sqrtf(bc2);

  int w_offset = 0;
  int b_offset = 0;

  for (int l = 1; l < nn->num_layers; l++) {
    int n_in = (l == 2 && nn->l2_input_size > nn->sizes[1]) ? nn->l2_input_size
                                                            : nn->sizes[l - 1];
    int n_out = nn->sizes[l];

    float *__restrict__ w = nn->weights[l];
    float *__restrict__ b = nn->biases[l];
    const float *__restrict__ d = nn->deltas[l];
    const float *__restrict__ prev_act = nn->activations[l - 1];

    float *__restrict__ mw = &nn->m_weight_buffer[w_offset];
    float *__restrict__ vw = &nn->v_weight_buffer[w_offset];
    float *__restrict__ mb = &nn->m_bias_buffer[b_offset];
    float *__restrict__ vb = &nn->v_bias_buffer[b_offset];

    for (int i = 0; i < n_out; i++) {
      float d_val = d[i];

      // --- Update bias with Adam ---
      float grad_b = d_val;
      mb[i] = ADAM_BETA1 * mb[i] + (1.0f - ADAM_BETA1) * grad_b;
      vb[i] = ADAM_BETA2 * vb[i] + (1.0f - ADAM_BETA2) * grad_b * grad_b;
      b[i] -=
          lr_corrected * mb[i] / (sqrtf(vb[i]) * bc2_sqrt_inv + ADAM_EPSILON);

      // --- Update weights with AdamW ---
      float *__restrict__ w_row = &w[i * n_in];
      float *__restrict__ mw_row = &mw[i * n_in];
      float *__restrict__ vw_row = &vw[i * n_in];

      for (int j = 0; j < n_in; j++) {
        float grad_w = d_val * prev_act[j];
        mw_row[j] = ADAM_BETA1 * mw_row[j] + (1.0f - ADAM_BETA1) * grad_w;
        vw_row[j] =
            ADAM_BETA2 * vw_row[j] + (1.0f - ADAM_BETA2) * grad_w * grad_w;

        // Adam update
        w_row[j] -= lr_corrected * mw_row[j] /
                    (sqrtf(vw_row[j]) * bc2_sqrt_inv + ADAM_EPSILON);
        // Decoupled weight decay
        w_row[j] -= learning_rate * weight_decay * w_row[j];
      }
    }

    w_offset += n_out * n_in;
    b_offset += n_out;
  }

  return loss;
}

void nn_free(NeuralNetwork *nn) {
  if (!nn)
    return;

  free(nn->sizes);
  free(nn->weight_buffer);
  free(nn->bias_buffer);
  free(nn->activation_buffer);
  free(nn->pre_activation_buffer);
  free(nn->delta_buffer);

  free(nn->weights);
  free(nn->biases);
  free(nn->activations);
  free(nn->pre_activations);
  free(nn->deltas);

  free(nn->quant_weight_buffer);
  free(nn->quant_bias_buffer);
  free(nn->quant_activation_buffer);
  free(nn->quant_weights);
  free(nn->quant_biases);
  free(nn->quant_activations);

  // Adam optimizer state (may be NULL if never trained)
  free(nn->m_weight_buffer);
  free(nn->m_bias_buffer);
  free(nn->v_weight_buffer);
  free(nn->v_bias_buffer);

  free(nn);
}

bool nn_save(const NeuralNetwork *nn, const char *filename) {
  if (!nn || !filename)
    return false;

  FILE *f = fopen(filename, "wb");
  if (!f)
    return false;

  // Write magic number: 'N', 'N', 'V', '2' (dual-perspective format)
  uint32_t magic = 0x4E4E5632;
  if (fwrite(&magic, sizeof(uint32_t), 1, f) != 1) {
    fclose(f);
    return false;
  }

  // Write l2_input_size for dual-perspective support
  if (fwrite(&nn->l2_input_size, sizeof(int), 1, f) != 1) {
    fclose(f);
    return false;
  }

  // Write num_layers
  if (fwrite(&nn->num_layers, sizeof(int), 1, f) != 1) {
    fclose(f);
    return false;
  }

  // Write sizes array
  if (fwrite(nn->sizes, sizeof(int), nn->num_layers, f) !=
      (size_t)nn->num_layers) {
    fclose(f);
    return false;
  }

  // Write weight buffer (includes enlarged layer 2 weights)
  if (fwrite(nn->weight_buffer, sizeof(float), nn->total_weights, f) !=
      (size_t)nn->total_weights) {
    fclose(f);
    return false;
  }

  // Write bias buffer
  if (fwrite(nn->bias_buffer, sizeof(float), nn->total_biases, f) !=
      (size_t)nn->total_biases) {
    fclose(f);
    return false;
  }

  fclose(f);
  return true;
}

bool nn_load(NeuralNetwork *nn, const char *filename) {
  if (!nn || !filename)
    return false;

  FILE *f = fopen(filename, "rb");
  if (!f)
    return false;

  // Read and verify magic number (NNV2 format)
  uint32_t magic = 0;
  if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != 0x4E4E5632) {
    fprintf(stderr,
            "Error: Incompatible weight file format (expected NNV2).\n");
    fclose(f);
    return false;
  }

  // Read and verify l2_input_size
  int file_l2_input = 0;
  if (fread(&file_l2_input, sizeof(int), 1, f) != 1 ||
      file_l2_input != nn->l2_input_size) {
    fprintf(stderr, "Error: l2_input_size mismatch (file=%d, expected=%d).\n",
            file_l2_input, nn->l2_input_size);
    fclose(f);
    return false;
  }

  // Read and verify num_layers
  int file_num_layers = 0;
  if (fread(&file_num_layers, sizeof(int), 1, f) != 1 ||
      file_num_layers != nn->num_layers) {
    fclose(f);
    return false;
  }

  // Read and verify sizes array
  int *file_sizes = (int *)malloc(file_num_layers * sizeof(int));
  if (!file_sizes) {
    fclose(f);
    return false;
  }

  if (fread(file_sizes, sizeof(int), file_num_layers, f) !=
      (size_t)file_num_layers) {
    free(file_sizes);
    fclose(f);
    return false;
  }

  for (int i = 0; i < file_num_layers; i++) {
    if (file_sizes[i] != nn->sizes[i]) {
      free(file_sizes);
      fclose(f);
      return false;
    }
  }
  free(file_sizes);

  // Read weight buffer (sized by total_weights which accounts for
  // l2_input_size)
  if (fread(nn->weight_buffer, sizeof(float), nn->total_weights, f) !=
      (size_t)nn->total_weights) {
    fclose(f);
    return false;
  }

  // Read bias buffer
  if (fread(nn->bias_buffer, sizeof(float), nn->total_biases, f) !=
      (size_t)nn->total_biases) {
    fclose(f);
    return false;
  }

  fclose(f);

  nn_quantize(nn);

  return true;
}

static inline int nnue_feature_index(Color perspective, Piece p, int sq,
                                     int ksq) {
  PieceType p_type = PIECE_TYPE(p);
  if (p_type == NONE || p_type == KING || p_type >= PIECE_TYPE_NB)
    return -1;

  Color p_color = PIECE_COLOR(p);
  int is_opponent = (p_color != perspective);
  int side_offset = is_opponent ? 5 : 0;
  int piece_idx = (int)p_type - 1; // 0..4 (PAWN..QUEEN)

  int oriented_sq = (perspective == WHITE) ? sq : (sq ^ 56);
  int oriented_ksq = (perspective == WHITE) ? ksq : (ksq ^ 56);

  int piece_sq_idx = (side_offset + piece_idx) * 64 + oriented_sq; // 0..639
  return oriented_ksq * 640 + piece_sq_idx;                        // 0..40959
}

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

void nn_quantize(NeuralNetwork *nn) {
  if (!nn)
    return;

  // Layer 1 weights and biases (Q1 = 128)
  int n_in = nn->sizes[0];
  int n_out = nn->sizes[1];

  for (int i = 0; i < n_out; i++) {
    for (int j = 0; j < n_in; j++) {
      float val = nn->weights[1][i * n_in + j] * 128.0f;
      if (val > 32767.0f)
        val = 32767.0f;
      if (val < -32768.0f)
        val = -32768.0f;
      nn->quant_weights[1][j * n_out + i] = (int16_t)roundf(val);
    }
  }
  for (int i = 0; i < n_out; i++) {
    nn->quant_biases[1][i] = (int32_t)roundf(nn->biases[1][i] * 128.0f);
  }

  // Subsequent layers (Qw = 64, Qb = 8192)
  for (int l = 2; l < nn->num_layers; l++) {
    n_in = (l == 2 && nn->l2_input_size > nn->sizes[1]) ? nn->l2_input_size
                                                        : nn->sizes[l - 1];
    n_out = nn->sizes[l];

    for (int i = 0; i < n_out * n_in; i++) {
      float val = nn->weights[l][i] * 64.0f;
      if (val > 32767.0f)
        val = 32767.0f;
      if (val < -32768.0f)
        val = -32768.0f;
      nn->quant_weights[l][i] = (int16_t)roundf(val);
    }
    for (int i = 0; i < n_out; i++) {
      nn->quant_biases[l][i] = (int32_t)roundf(nn->biases[l][i] * 8192.0f);
    }
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

static int extract_features_for_perspective(const Position *pos,
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

int nn_extract_active_features(const Position *pos, int *active_indices) {
  return extract_features_for_perspective(pos, pos->sideToMove, active_indices);
}

void nn_extract_active_features_dual(const Position *pos, int *stm_indices,
                                     int *stm_count, int *opp_indices,
                                     int *opp_count) {
  if (!pos)
    return;
  Color stm = pos->sideToMove;
  Color opp = OPPOSITE(stm);
  if (stm_indices && stm_count) {
    *stm_count = extract_features_for_perspective(pos, stm, stm_indices);
  }
  if (opp_indices && opp_count) {
    *opp_count = extract_features_for_perspective(pos, opp, opp_indices);
  }
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

  if (!nn->adam_initialized) {
    if (!nn_adam_init(nn))
      return NAN;
  }

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
