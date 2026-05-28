#include "nn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

NeuralNetwork* nn_init(const int *sizes, int num_layers) {
    if (!sizes || num_layers < 2) {
        return NULL;
    }
    if (sizes[num_layers - 1] != 1) {
        return NULL; // Must have exactly one output neuron
    }

    NeuralNetwork *nn = (NeuralNetwork*)malloc(sizeof(NeuralNetwork));
    if (!nn) return NULL;

    nn->num_layers = num_layers;
    nn->sizes = (int*)malloc(num_layers * sizeof(int));
    if (!nn->sizes) {
        free(nn);
        return NULL;
    }
    memcpy(nn->sizes, sizes, num_layers * sizeof(int));

    // Calculate total buffer sizes needed
    nn->total_weights = 0;
    nn->total_biases = 0;
    nn->total_neurons = 0;
    nn->total_post_input_neurons = 0;

    for (int l = 0; l < num_layers; l++) {
        nn->total_neurons += sizes[l];
        if (l > 0) {
            nn->total_weights += sizes[l] * sizes[l - 1];
            nn->total_biases += sizes[l];
            nn->total_post_input_neurons += sizes[l];
        }
    }

    // Allocate memory blocks (use calloc to zero activations, biases, and deltas)
    nn->weight_buffer = (float*)malloc(nn->total_weights * sizeof(float));
    nn->bias_buffer = (float*)calloc(nn->total_biases, sizeof(float));
    nn->activation_buffer = (float*)calloc(nn->total_neurons, sizeof(float));
    nn->pre_activation_buffer = (float*)calloc(nn->total_post_input_neurons, sizeof(float));
    nn->delta_buffer = (float*)calloc(nn->total_post_input_neurons, sizeof(float));

    // Allocate pointer arrays
    nn->weights = (float**)malloc(num_layers * sizeof(float*));
    nn->biases = (float**)malloc(num_layers * sizeof(float*));
    nn->activations = (float**)malloc(num_layers * sizeof(float*));
    nn->pre_activations = (float**)malloc(num_layers * sizeof(float*));
    nn->deltas = (float**)malloc(num_layers * sizeof(float*));

    // Check if any allocations failed
    if (!nn->weight_buffer || !nn->bias_buffer || !nn->activation_buffer ||
        !nn->pre_activation_buffer || !nn->delta_buffer || !nn->weights ||
        !nn->biases || !nn->activations || !nn->pre_activations || !nn->deltas) {
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
        nn->weights[l] = w_ptr;
        w_ptr += sizes[l] * sizes[l - 1];

        nn->biases[l] = b_ptr;
        b_ptr += sizes[l];

        nn->activations[l] = a_ptr;
        a_ptr += sizes[l];

        nn->pre_activations[l] = z_ptr;
        z_ptr += sizes[l];

        nn->deltas[l] = d_ptr;
        d_ptr += sizes[l];
    }

    // Initialize weights using He / Xavier initialization
    // Use a local deterministic PRNG (Xorshift32) to ensure reproducibility
    uint32_t rng_state = 314159265; // Pi seed
    for (int l = 1; l < num_layers; l++) {
        int n_in = sizes[l - 1];
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

    return nn;
}

float nn_forward(NeuralNetwork *nn, const float *inputs) {
    if (!nn || !inputs) return 0.0f;

    // Copy inputs to the input layer (layer 0 activations)
    memcpy(nn->activations[0], inputs, nn->sizes[0] * sizeof(float));

    // Propagate forward through layers
    for (int l = 1; l < nn->num_layers; l++) {
        int n_in = nn->sizes[l - 1];
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
            float32x4_t total_vec = vaddq_f32(
                vaddq_f32(sum_vec0, sum_vec1),
                vaddq_f32(sum_vec2, sum_vec3)
            );
            sum += vaddvq_f32(total_vec);
#endif
            for (; j < n_in; j++) {
                sum += w_row[j] * prev_act[j];
            }

            pre_act[i] = sum;

            // Activation functions
            if (l < nn->num_layers - 1) {
                // ReLU for hidden layers
                act[i] = (sum > 0.0f) ? sum : 0.0f;
            } else {
                // Linear (Identity) activation for the single output neuron
                act[i] = sum;
            }
        }
    }

    // Return the single scalar output from the final layer
    return nn->activations[nn->num_layers - 1][0];
}

float nn_train_step(NeuralNetwork *nn, const float *inputs, float target, float learning_rate) {
    if (!nn || !inputs) return 0.0f;

    // 1. Forward Propagation
    float output = nn_forward(nn, inputs);

    // Compute Loss (Mean Squared Error: L = 0.5 * (output - target)^2)
    float error = output - target;
    float loss = 0.5f * error * error;

    // 2. Backward Propagation (Compute Deltas)
    int L = nn->num_layers - 1;

    // Output Layer delta: derivative of loss * derivative of linear output activation (1.0)
    nn->deltas[L][0] = error;

    // Backpropagate deltas through hidden layers
    for (int l = L - 1; l >= 1; l--) {
        int n_curr = nn->sizes[l];
        int n_next = nn->sizes[l + 1];

        float *__restrict__ d_curr = nn->deltas[l];
        const float *__restrict__ d_next = nn->deltas[l + 1];
        const float *__restrict__ w_next = nn->weights[l + 1];
        const float *__restrict__ z_curr = nn->pre_activations[l];

        // Zero out current layer delta first (swapped loops)
        memset(d_curr, 0, n_curr * sizeof(float));

        for (int j = 0; j < n_next; j++) {
            float d_val = d_next[j];
            const float *__restrict__ w_row = &w_next[j * n_curr];
            
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

        // Apply ReLU derivative: delta is 0 if z <= 0
        for (int i = 0; i < n_curr; i++) {
            if (z_curr[i] <= 0.0f) {
                d_curr[i] = 0.0f;
            }
        }
    }

    // 3. Update Weights and Biases (Stochastic Gradient Descent)
    for (int l = 1; l < nn->num_layers; l++) {
        int n_in = nn->sizes[l - 1];
        int n_out = nn->sizes[l];

        float *__restrict__ w = nn->weights[l];
        float *__restrict__ b = nn->biases[l];
        const float *__restrict__ d = nn->deltas[l];
        const float *__restrict__ prev_act = nn->activations[l - 1];

        for (int i = 0; i < n_out; i++) {
            float d_val = d[i];
            
            // Update bias
            b[i] -= learning_rate * d_val;

            // Update weights row
            float *__restrict__ w_row = &w[i * n_in];
            float lr_d_val = learning_rate * d_val;
            
            int j = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
            float32x4_t lr_d_vec = vdupq_n_f32(lr_d_val);
            for (; j <= n_in - 16; j += 16) {
                float32x4_t w_vec0 = vld1q_f32(&w_row[j]);
                float32x4_t a_vec0 = vld1q_f32(&prev_act[j]);
                float32x4_t res_vec0 = vmlsq_f32(w_vec0, lr_d_vec, a_vec0);
                vst1q_f32(&w_row[j], res_vec0);

                float32x4_t w_vec1 = vld1q_f32(&w_row[j + 4]);
                float32x4_t a_vec1 = vld1q_f32(&prev_act[j + 4]);
                float32x4_t res_vec1 = vmlsq_f32(w_vec1, lr_d_vec, a_vec1);
                vst1q_f32(&w_row[j + 4], res_vec1);

                float32x4_t w_vec2 = vld1q_f32(&w_row[j + 8]);
                float32x4_t a_vec2 = vld1q_f32(&prev_act[j + 8]);
                float32x4_t res_vec2 = vmlsq_f32(w_vec2, lr_d_vec, a_vec2);
                vst1q_f32(&w_row[j + 8], res_vec2);

                float32x4_t w_vec3 = vld1q_f32(&w_row[j + 12]);
                float32x4_t a_vec3 = vld1q_f32(&prev_act[j + 12]);
                float32x4_t res_vec3 = vmlsq_f32(w_vec3, lr_d_vec, a_vec3);
                vst1q_f32(&w_row[j + 12], res_vec3);
            }
            for (; j <= n_in - 4; j += 4) {
                float32x4_t w_vec = vld1q_f32(&w_row[j]);
                float32x4_t a_vec = vld1q_f32(&prev_act[j]);
                float32x4_t res_vec = vmlsq_f32(w_vec, lr_d_vec, a_vec);
                vst1q_f32(&w_row[j], res_vec);
            }
#endif
            for (; j < n_in; j++) {
                w_row[j] -= lr_d_val * prev_act[j];
            }
        }
    }

    return loss;
}

void nn_free(NeuralNetwork *nn) {
    if (!nn) return;

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

    free(nn);
}

bool nn_save(const NeuralNetwork *nn, const char *filename) {
    if (!nn || !filename) return false;

    FILE *f = fopen(filename, "wb");
    if (!f) return false;

    // Write magic number: 'N', 'N', 'W', 'T'
    uint32_t magic = 0x4E4E5754;
    if (fwrite(&magic, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return false;
    }

    // Write num_layers
    if (fwrite(&nn->num_layers, sizeof(int), 1, f) != 1) {
        fclose(f);
        return false;
    }

    // Write sizes array
    if (fwrite(nn->sizes, sizeof(int), nn->num_layers, f) != (size_t)nn->num_layers) {
        fclose(f);
        return false;
    }

    // Write weight buffer
    if (fwrite(nn->weight_buffer, sizeof(float), nn->total_weights, f) != (size_t)nn->total_weights) {
        fclose(f);
        return false;
    }

    // Write bias buffer
    if (fwrite(nn->bias_buffer, sizeof(float), nn->total_biases, f) != (size_t)nn->total_biases) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

bool nn_load(NeuralNetwork *nn, const char *filename) {
    if (!nn || !filename) return false;

    FILE *f = fopen(filename, "rb");
    if (!f) return false;

    // Read and verify magic number
    uint32_t magic = 0;
    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != 0x4E4E5754) {
        fclose(f);
        return false;
    }

    // Read and verify num_layers
    int file_num_layers = 0;
    if (fread(&file_num_layers, sizeof(int), 1, f) != 1 || file_num_layers != nn->num_layers) {
        fclose(f);
        return false;
    }

    // Read and verify sizes array
    int *file_sizes = (int*)malloc(file_num_layers * sizeof(int));
    if (!file_sizes) {
        fclose(f);
        return false;
    }

    if (fread(file_sizes, sizeof(int), file_num_layers, f) != (size_t)file_num_layers) {
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

    // Read weight buffer
    if (fread(nn->weight_buffer, sizeof(float), nn->total_weights, f) != (size_t)nn->total_weights) {
        fclose(f);
        return false;
    }

    // Read bias buffer
    if (fread(nn->bias_buffer, sizeof(float), nn->total_biases, f) != (size_t)nn->total_biases) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

void nn_extract_features(const Position *pos, float *features) {
    if (!pos || !features) return;
    
    memset(features, 0, 768 * sizeof(float));
    Color stm = pos->sideToMove;
    
    for (int sq = 0; sq < 64; sq++) {
        Piece p = pos->board[sq];
        if (p == EMPTY) continue;
        
        Color p_color = PIECE_COLOR(p);
        PieceType p_type = PIECE_TYPE(p);
        
        if (p_type == NONE || p_type >= PIECE_TYPE_NB) continue;
        
        int is_opponent = (p_color != stm);
        int side_offset = is_opponent ? 6 : 0;
        int piece_idx = (int)p_type - 1;
        
        // Orient the square vertically from side-to-move's perspective
        int oriented_sq = (stm == WHITE) ? sq : (sq ^ 56);
        
        int feature_idx = (side_offset + piece_idx) * 64 + oriented_sq;
        features[feature_idx] = 1.0f;
    }
}

