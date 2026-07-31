#include "nn.h"
#include <math.h>
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

  // Initialize all buffers to NULL to ensure safe cleanup if an allocation fails
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

  // Detect NNUE dual-perspective mode: layer 2 input width is 2*accumulator size
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
