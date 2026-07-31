#ifndef NN_H
#define NN_H

#include "boards.h"

/**
 * @struct NeuralNetwork
 * @brief Structure representing a Feedforward Neural Network optimized for cache locality.
 * 
 * All weight, bias, activation, and delta arrays are allocated in contiguous blocks
 * to minimize pointer-chasing and improve cache efficiency.
 */
typedef struct {
    int num_layers;             ///< Total number of layers (input + hidden + output)
    int *sizes;                 ///< Sizes of each layer (allocated contiguously)

    // Contiguous memory buffers for all network parameters and states
    float *weight_buffer;       ///< Contiguous block storing all weights
    float *bias_buffer;         ///< Contiguous block storing all biases
    float *activation_buffer;   ///< Contiguous block storing all activations (outputs of layers)
    float *pre_activation_buffer; ///< Contiguous block storing all pre-activation values (z)
    float *delta_buffer;        ///< Contiguous block storing all backpropagation deltas

    // Layer-wise pointers into the contiguous buffers above
    float **weights;            ///< weights[l] points to weight matrix of layer l (1 <= l < num_layers)
    float **biases;             ///< biases[l] points to bias vector of layer l (1 <= l < num_layers)
    float **activations;        ///< activations[l] points to activation vector of layer l (0 <= l < num_layers)
    float **pre_activations;    ///< pre_activations[l] points to pre-activation vector of layer l (1 <= l < num_layers)
    float **deltas;             ///< deltas[l] points to delta vector of layer l (1 <= l < num_layers)
    
    // Contiguous memory buffers for integer quantized parameters
    int16_t *quant_weight_buffer;   ///< Contiguous block storing all quantized weights
    int32_t *quant_bias_buffer;     ///< Contiguous block storing all quantized biases
    int32_t *quant_activation_buffer; ///< Contiguous block storing all quantized activations
    
    // Layer-wise pointers into the contiguous quantized buffers above
    int16_t **quant_weights;        ///< quant_weights[l] points to quantized weight matrix of layer l
    int32_t **quant_biases;         ///< quant_biases[l] points to quantized bias vector of layer l
    int32_t **quant_activations;    ///< quant_activations[l] points to quantized activation vector of layer l

    // Adam optimizer state buffers (allocated only when training)
    float *m_weight_buffer;     ///< First moment (momentum) for weights
    float *m_bias_buffer;       ///< First moment (momentum) for biases
    float *v_weight_buffer;     ///< Second moment (variance) for weights
    float *v_bias_buffer;       ///< Second moment (variance) for biases
    int adam_initialized;       ///< Whether Adam buffers have been allocated
    int train_step_count;       ///< Global step counter for Adam bias correction

    int total_weights;          ///< Total number of weights in the network
    int total_biases;           ///< Total number of biases in the network
    int total_neurons;          ///< Total number of neurons in all layers (including input)
    int total_post_input_neurons; ///< Total number of neurons in hidden + output layers
    int l2_input_size;          ///< Actual input width for layer 2 (NN_L2_INPUT_SIZE for NNUE, else sizes[1])
} NeuralNetwork;

/**
 * @brief Initializes a feedforward neural network with the specified layer sizes.
 * 
 * Weights are initialized using He (Kaiming) initialization for layers feeding into 
 * ReLU activations, and Xavier (Glorot) initialization for the output layer.
 * 
 * @param sizes Array containing the number of neurons in each layer.
 * @param num_layers Total number of layers in the network.
 * @return A pointer to the initialized NeuralNetwork, or NULL on failure.
 */
NeuralNetwork* nn_init(const int *sizes, int num_layers);

/**
 * @brief Performs forward propagation through the network.
 * 
 * Hidden layers use the ReLU activation function. The single output neuron in the
 * output layer uses a linear (identity) activation function, which makes it suitable
 * for regression targets (like evaluation scores).
 * 
 * @param nn Pointer to the NeuralNetwork.
 * @param inputs Array of size equal to sizes[0] containing input features.
 * @return The output of the single output neuron in the final layer.
 */
float nn_forward(NeuralNetwork *nn, const float *inputs);

/**
 * @brief Unified training step: forward propagation, backpropagation, and weight update.
 * 
 * Uses Mean Squared Error (MSE) loss: L = 0.5 * (output - target)^2.
 * Calculates gradients and updates the network weights and biases using
 * the Adam optimizer with decoupled weight decay (AdamW).
 * 
 * Adam state buffers are lazily allocated on the first call.
 * 
 * @param nn Pointer to the NeuralNetwork.
 * @param inputs Array of size equal to sizes[0] containing input features.
 * @param target The target scalar value.
 * @param learning_rate The learning rate for the weight update step.
 * @param weight_decay Decoupled weight decay coefficient (0 to disable).
 * @return The loss before the weight update.
 */
float nn_train_step(NeuralNetwork *nn, const float *inputs, float target, float learning_rate, float weight_decay);

#include <stdbool.h>

/**
 * @brief Safely frees all memory allocated for the neural network.
 * 
 * @param nn Pointer to the NeuralNetwork.
 */
void nn_free(NeuralNetwork *nn);

/**
 * @brief Saves the neural network weights and biases to a file.
 * 
 * Saves the architecture configuration (number of layers and sizes) followed
 * by the weights and biases in a binary format.
 * 
 * @param nn Pointer to the NeuralNetwork.
 * @param filename Path to the output file.
 * @return true on success, false on failure.
 */
bool nn_save(const NeuralNetwork *nn, const char *filename);

/**
 * @brief Loads the neural network weights and biases from a file.
 * 
 * Before loading, verifies that the architecture in the file (number of layers
 * and layer sizes) exactly matches the network's current architecture.
 * 
 * @param nn Pointer to the NeuralNetwork.
 * @param filename Path to the input file.
 * @return true on success, false on failure.
 */
bool nn_load(NeuralNetwork *nn, const char *filename);

/**
 * @brief Performs on-the-fly quantization of float parameters to integer parameters.
 * 
 * @param nn Pointer to the NeuralNetwork.
 */
void nn_quantize(NeuralNetwork *nn);

#define NN_INPUT_SIZE 40972
#define NN_ACCUM_SIZE 256
#define NN_HIDDEN_SIZE 32
#define NN_L2_INPUT_SIZE (NN_ACCUM_SIZE * 2)  /* 512: concatenated [stm_accum | opp_accum] */

static inline int nnue_feature_index(Color perspective, Piece p, int sq, int ksq) {
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

/**
 * @brief Extracts HalfKA (King-relative piece-square + castling/EP state) features from a chess position.
 * 
 * Maps 64 king squares x 640 non-king piece features to 40,960 elements, followed by 4 castling features
 * and 8 en-passant file features (total NN_INPUT_SIZE = 40,972 elements).
 * The representation is oriented from the side-to-move's perspective.
 * 
 * @param pos Pointer to the chess Position structure.
 * @param features Destination float array of size NN_INPUT_SIZE.
 */
void nn_extract_features(const Position *pos, float *features);

/**
 * @brief Fully recalculates the accumulators for White and Black perspectives from scratch.
 */
void nnue_refresh_accumulator(NeuralNetwork *nn, Position *pos);

struct Undo;

void nnue_update_accumulator(NeuralNetwork *nn, Position *pos, Move m, const struct Undo *u);

/**
 * @brief Incrementally rolls back the accumulators for White and Black perspectives after undoing a move.
 */
void nnue_undo_accumulator(NeuralNetwork *nn, Position *pos, Move m, const struct Undo *u);

/**
 * @brief Evaluates the position starting from the cached accumulators.
 * @details The fixed-point output is in units of 1/8192 pawn.
 */
int32_t nnue_evaluate_accumulator(NeuralNetwork *nn, const Position *pos);

/**
 * @struct TrainingSample
 * @brief Represents a single chess training position and scalar evaluation target.
 */
typedef struct {
    CompactPosition pos;
    float target;
} TrainingSample;

void nn_extract_active_features_dual_compact(const CompactPosition *pos, int *stm_indices, int *stm_count, int *opp_indices, int *opp_count);

/**
 * @brief Trains the network on a single position using dual-perspective features.
 * @details Extracts features from both perspectives, runs forward/backward, and applies AdamW update.
 * @param nn Pointer to the NeuralNetwork.
 * @param pos Pointer to the chess Position.
 * @param target The target scalar value.
 * @param learning_rate The learning rate.
 * @param weight_decay Weight decay coefficient.
 * @return The loss before the weight update.
 */
float nn_train_step_pos(NeuralNetwork *nn, const Position *pos, float target, float learning_rate, float weight_decay);

typedef struct NNBatchTrainer NNBatchTrainer;

/**
 * @brief Initializes multi-threaded batch trainer contexts.
 * @param nn Network structure prototype.
 * @param num_threads Number of worker threads to spawn for training/eval.
 * @return Pointer to NNBatchTrainer struct, or NULL on failure.
 */
NNBatchTrainer* nn_batch_trainer_init(NeuralNetwork *nn, int num_threads);

/**
 * @brief Frees all memory allocated for NNBatchTrainer.
 */
void nn_batch_trainer_free(NNBatchTrainer *trainer);

/**
 * @brief Performs multi-threaded parallel mini-batch training step.
 * Computes forward pass, loss, deltas, and sparse gradient accumulation across threads.
 * Applies AdamW parameter update ONCE for the batch on the main network.
 * 
 * @param trainer Pointer to NNBatchTrainer.
 * @param nn Pointer to NeuralNetwork.
 * @param samples Array of TrainingSample objects in this batch.
 * @param batch_size Number of samples in the batch.
 * @param learning_rate Learning rate for AdamW update step.
 * @param weight_decay Decoupled weight decay coefficient.
 * @return Total loss sum for all samples in the batch.
 */
float nn_train_batch_parallel(NNBatchTrainer *trainer, NeuralNetwork *nn, const TrainingSample *samples, int batch_size, float learning_rate, float weight_decay);

/**
 * @brief Performs multi-threaded validation loss evaluation on float network and quantized network.
 */
float nn_evaluate_batch_parallel(NNBatchTrainer *trainer, NeuralNetwork *nn, const TrainingSample *samples, int num_samples, float *out_quant_loss);

#endif /* NN_H */
