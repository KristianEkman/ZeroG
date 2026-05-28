#include "unity.h"
#include "nn.h"
#include <stdio.h>
#include <math.h>

/* ── Unity boilerplate ───────────────────────────────────────────────────── */
void setUp(void) {}
void tearDown(void) {}

/* ── Test cases ──────────────────────────────────────────────────────────── */

void test_nn_init_and_free(void)
{
    int sizes[] = {3, 4, 1};
    NeuralNetwork *nn = nn_init(sizes, 3);
    
    TEST_ASSERT_NOT_NULL(nn);
    TEST_ASSERT_EQUAL_INT(3, nn->num_layers);
    TEST_ASSERT_EQUAL_INT(3, nn->sizes[0]);
    TEST_ASSERT_EQUAL_INT(4, nn->sizes[1]);
    TEST_ASSERT_EQUAL_INT(1, nn->sizes[2]);

    // Check pointer validity
    TEST_ASSERT_NOT_NULL(nn->weights[1]);
    TEST_ASSERT_NOT_NULL(nn->weights[2]);
    TEST_ASSERT_NOT_NULL(nn->biases[1]);
    TEST_ASSERT_NOT_NULL(nn->biases[2]);
    TEST_ASSERT_NOT_NULL(nn->activations[0]);
    TEST_ASSERT_NOT_NULL(nn->activations[1]);
    TEST_ASSERT_NOT_NULL(nn->activations[2]);

    // Check initialization of weights (non-zero check)
    int non_zero_count = 0;
    int total_weights = (3 * 4) + (4 * 1);
    for (int i = 0; i < total_weights; i++) {
        if (fabsf(nn->weight_buffer[i]) > 1e-6f) {
            non_zero_count++;
        }
    }
    TEST_ASSERT_EQUAL_INT(total_weights, non_zero_count);

    // Check invalid configurations (last layer must be 1)
    int invalid_sizes[] = {3, 4, 2};
    NeuralNetwork *nn_invalid = nn_init(invalid_sizes, 3);
    TEST_ASSERT_NULL(nn_invalid);

    nn_free(nn);
}

void test_nn_forward_returns_finite(void)
{
    int sizes[] = {2, 5, 1};
    NeuralNetwork *nn = nn_init(sizes, 3);
    TEST_ASSERT_NOT_NULL(nn);

    float inputs[] = {0.5f, -0.2f};
    float output = nn_forward(nn, inputs);

    // Verify it is a valid floating point number and not NaN
    TEST_ASSERT_FALSE(isnan(output));
    TEST_ASSERT_FALSE(isinf(output));

    nn_free(nn);
}

void test_nn_train_xor_gate(void)
{
    // XOR inputs and outputs
    float inputs[4][2] = {
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f}
    };
    float targets[4] = {0.0f, 1.0f, 1.0f, 0.0f};

    // Initialize 3-layer neural network (2 input, 8 hidden, 1 output)
    int sizes[] = {2, 8, 1};
    NeuralNetwork *nn = nn_init(sizes, 3);
    TEST_ASSERT_NOT_NULL(nn);

    float lr = 0.1f;
    float initial_total_loss = 0.0f;
    
    // Evaluate initial loss
    for (int i = 0; i < 4; i++) {
        float out = nn_forward(nn, inputs[i]);
        float diff = out - targets[i];
        initial_total_loss += 0.5f * diff * diff;
    }

    // Train for 5000 epochs
    for (int epoch = 0; epoch < 5000; epoch++) {
        for (int i = 0; i < 4; i++) {
            nn_train_step(nn, inputs[i], targets[i], lr);
        }
    }

    float final_total_loss = 0.0f;
    printf("\n=== XOR Training Results ===\n");
    for (int i = 0; i < 4; i++) {
        float out = nn_forward(nn, inputs[i]);
        float diff = out - targets[i];
        final_total_loss += 0.5f * diff * diff;
        printf("Input: [%.1f, %.1f] -> Target: %.1f, Predicted: %+.4f (Error: %+.4f)\n",
               inputs[i][0], inputs[i][1], targets[i], out, diff);
        
        // Assert predictions are very close to targets
        TEST_ASSERT_FLOAT_WITHIN(0.15f, targets[i], out);
    }
    printf("Initial Loss: %.6f, Final Loss: %.6f\n", initial_total_loss, final_total_loss);
    printf("============================\n");

    // Assert loss decreased dramatically
    TEST_ASSERT_TRUE(final_total_loss < initial_total_loss);
    TEST_ASSERT_TRUE(final_total_loss < 0.02f);

    nn_free(nn);
}

void test_nn_train_large_network(void)
{
    // Initialize 4-layer neural network (50 input, 50 hidden, 50 hidden, 1 output)
    int sizes[] = {50, 50, 50, 1};
    NeuralNetwork *nn = nn_init(sizes, 4);
    TEST_ASSERT_NOT_NULL(nn);

    int num_samples = 16;
    float inputs[16][50];
    float targets[16];

    // Generate deterministic input data and targets using sine/cosine relationships
    for (int i = 0; i < num_samples; i++) {
        for (int j = 0; j < 50; j++) {
            inputs[i][j] = (float)sin(i * 1.3f + j * 0.7f);
        }
        targets[i] = 0.5f * inputs[i][0] - 0.3f * inputs[i][1] + 0.8f * inputs[i][2] * inputs[i][2] - 0.4f * inputs[i][3];
    }

    float lr = 0.01f;
    float initial_total_loss = 0.0f;
    
    // Evaluate initial loss
    for (int i = 0; i < num_samples; i++) {
        float out = nn_forward(nn, inputs[i]);
        float diff = out - targets[i];
        initial_total_loss += 0.5f * diff * diff;
    }

    // Train for 1000 epochs
    for (int epoch = 0; epoch < 1000; epoch++) {
        for (int i = 0; i < num_samples; i++) {
            nn_train_step(nn, inputs[i], targets[i], lr);
        }
    }

    float final_total_loss = 0.0f;
    printf("\n=== Large Network (50,50,50,1) Training Results ===\n");
    for (int i = 0; i < num_samples; i++) {
        float out = nn_forward(nn, inputs[i]);
        float diff = out - targets[i];
        final_total_loss += 0.5f * diff * diff;
        
        // Assert prediction is very close to target (within 0.05)
        TEST_ASSERT_FLOAT_WITHIN(0.05f, targets[i], out);
    }
    printf("Initial Loss: %.6f, Final Loss: %.6f\n", initial_total_loss, final_total_loss);
    printf("===================================================\n");

    // Assert loss decreased dramatically and is extremely small
    TEST_ASSERT_TRUE(final_total_loss < initial_total_loss);
    TEST_ASSERT_TRUE(final_total_loss < 0.001f);

    nn_free(nn);
}

void test_nn_save_load(void)
{
    const char *test_file = "test_weights.bin";
    int sizes[] = {2, 3, 1};

    // 1. Create and initialize nn1
    NeuralNetwork *nn1 = nn_init(sizes, 3);
    TEST_ASSERT_NOT_NULL(nn1);

    // 2. Perform a forward pass on nn1 to get reference output
    float inputs[] = {0.5f, -0.5f};
    float out1 = nn_forward(nn1, inputs);

    // 3. Save weights of nn1
    TEST_ASSERT_TRUE(nn_save(nn1, test_file));

    // 4. Create nn2 (same architecture, different random weights)
    NeuralNetwork *nn2 = nn_init(sizes, 3);
    TEST_ASSERT_NOT_NULL(nn2);
    
    // Verify that nn2 initial output differs or is at least independent
    float out2_initial = nn_forward(nn2, inputs);
    (void)out2_initial; // just computed to populate internal state

    // 5. Load weights into nn2
    TEST_ASSERT_TRUE(nn_load(nn2, test_file));

    // 6. Verify outputs and weights match exactly
    float out2_loaded = nn_forward(nn2, inputs);
    TEST_ASSERT_EQUAL_FLOAT(out1, out2_loaded);

    for (int i = 0; i < nn1->total_weights; i++) {
        TEST_ASSERT_EQUAL_FLOAT(nn1->weight_buffer[i], nn2->weight_buffer[i]);
    }
    for (int i = 0; i < nn1->total_biases; i++) {
        TEST_ASSERT_EQUAL_FLOAT(nn1->bias_buffer[i], nn2->bias_buffer[i]);
    }

    // 7. Verify loader prevents loading into wrong architecture
    int different_sizes[] = {2, 4, 1};
    NeuralNetwork *nn3 = nn_init(different_sizes, 3);
    TEST_ASSERT_NOT_NULL(nn3);

    // Should fail because hidden layer sizes differ (3 vs 4)
    TEST_ASSERT_FALSE(nn_load(nn3, test_file));

    // 8. Clean up
    nn_free(nn1);
    nn_free(nn2);
    nn_free(nn3);
    remove(test_file);
}

/* ── main (Unity runner) ──────────────────────────────────────────────── */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_nn_init_and_free);
    RUN_TEST(test_nn_forward_returns_finite);
    RUN_TEST(test_nn_train_xor_gate);
    RUN_TEST(test_nn_train_large_network);
    RUN_TEST(test_nn_save_load);

    return UNITY_END();
}
