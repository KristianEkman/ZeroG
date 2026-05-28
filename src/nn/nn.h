#ifndef NN_H
#define NN_H

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

    int total_weights;          ///< Total number of weights in the network
    int total_biases;           ///< Total number of biases in the network
    int total_neurons;          ///< Total number of neurons in all layers (including input)
    int total_post_input_neurons; ///< Total number of neurons in hidden + output layers
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
 * Stochastic Gradient Descent (SGD) with the specified learning rate.
 * 
 * @param nn Pointer to the NeuralNetwork.
 * @param inputs Array of size equal to sizes[0] containing input features.
 * @param target The target scalar value.
 * @param learning_rate The learning rate for the weight update step.
 * @return The loss before the weight update.
 */
float nn_train_step(NeuralNetwork *nn, const float *inputs, float target, float learning_rate);

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

#endif /* NN_H */
