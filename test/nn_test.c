#include "unity.h"
#include "nn.h"
#include "boards.h"
#include "fen.h"
#include "movegen.h"
#include "uci.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

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

void test_nn_feature_extraction(void)
{
    Position pos;
    // Initialize starting position
    position_startpos(&pos);

    float features[768];
    nn_extract_features(&pos, features);

    // Starting position has 32 pieces:
    // White: 8 pawns, 2 knights, 2 bishops, 2 rooks, 1 queen, 1 king (16 pieces)
    // Black: 8 pawns, 2 knights, 2 bishops, 2 rooks, 1 queen, 1 king (16 pieces)
    // Side to move is White.
    // Active features should be 32 (1.0f values).
    int active_count = 0;
    for (int i = 0; i < 768; i++) {
        if (features[i] == 1.0f) {
            active_count++;
        } else {
            TEST_ASSERT_EQUAL_FLOAT(0.0f, features[i]);
        }
    }
    TEST_ASSERT_EQUAL_INT(32, active_count);

    // Let's verify friendly pieces count is 16 and opponent pieces count is 16
    int friendly_count = 0;
    int opponent_count = 0;
    for (int i = 0; i < 384; i++) {
        if (features[i] == 1.0f) friendly_count++;
    }
    for (int i = 384; i < 768; i++) {
        if (features[i] == 1.0f) opponent_count++;
    }
    TEST_ASSERT_EQUAL_INT(16, friendly_count);
    TEST_ASSERT_EQUAL_INT(16, opponent_count);

    // Change side to move to Black and check perspective change
    pos.sideToMove = BLACK;
    float black_features[768];
    nn_extract_features(&pos, black_features);

    // Black is now friendly, White is opponent.
    // Ensure total active count is still 32
    active_count = 0;
    for (int i = 0; i < 768; i++) {
        if (black_features[i] == 1.0f) {
            active_count++;
        }
    }
    TEST_ASSERT_EQUAL_INT(32, active_count);

    // Let's make sure that friendly pieces for Black are at the same locations (except mirrored ranks)
    // For example, friendly pawns for Black on starting board are on A7..H7.
    // From Black's perspective, they should be on rank 2 (A2..H2, squares 8..15).
    // Let's check features[8..15] (friendly pawns on rank 2 - should be 8 pawns!).
    int black_friendly_pawn_count_on_rank2 = 0;
    for (int sq = 8; sq < 16; sq++) {
        // Friendly pawn index: piece_idx=0 (PAWN-1), side_offset=0
        // feature index = 0 * 64 + sq
        if (black_features[sq] == 1.0f) {
            black_friendly_pawn_count_on_rank2++;
        }
    }
    TEST_ASSERT_EQUAL_INT(8, black_friendly_pawn_count_on_rank2);
}

void test_nnue_incremental_correctness(void)
{
    // Parse starting position
    Position pos;
    memset(&pos, 0, sizeof(Position));
    TEST_ASSERT_EQUAL_INT(0, fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &pos));

    // Initialize NN
    int sizes[] = {768, 64, 32, 1};
    NeuralNetwork *nn = nn_init(sizes, 4);
    TEST_ASSERT_NOT_NULL(nn);

    // Refresh accumulator
    nnue_refresh_accumulator(nn, &pos);

    // Compare full forward vs accumulator evaluation
    float features[768];
    nn_extract_features(&pos, features);
    float full_output = nn_forward(nn, features);
    float accum_output = nnue_evaluate_accumulator(nn, &pos);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, full_output, accum_output);

    // Make some moves and verify incremental update matches full refresh at each step
    Move moves[MAX_MOVES];
    int count = movegen_legal(&pos, moves);
    TEST_ASSERT_TRUE(count > 0);

    extern NeuralNetwork *eval_nn;
    extern bool use_nn;

    // Try a few moves
    for (int i = 0; i < count && i < 10; i++) {
        Undo u;
        NeuralNetwork *old_eval_nn = eval_nn;
        bool old_use_nn = use_nn;
        eval_nn = nn;
        use_nn = true;

        Position next_pos_auto = pos;
        apply_move(&next_pos_auto, moves[i], &u);

        // Reset global pointers
        eval_nn = old_eval_nn;
        use_nn = old_use_nn;

        // Fully refresh accumulator on a clean copy of next_pos_auto
        Position next_pos_refreshed = next_pos_auto;
        nnue_refresh_accumulator(nn, &next_pos_refreshed);

        // Check White and Black accumulators match exactly
        for (int side = 0; side < 2; side++) {
            for (int k = 0; k < 64; k++) {
                TEST_ASSERT_EQUAL_INT(next_pos_refreshed.accum[side][k], next_pos_auto.accum[side][k]);
            }
        }

        // Compare evaluate outputs
        float output_auto = nnue_evaluate_accumulator(nn, &next_pos_auto);
        float output_refreshed = nnue_evaluate_accumulator(nn, &next_pos_refreshed);
        TEST_ASSERT_EQUAL_FLOAT(output_refreshed, output_auto);
    }

    nn_free(nn);
}

static void nnue_test_incremental_recursive(NeuralNetwork *nn, Position *pos, int depth) {
    if (depth == 0) return;
    
    Move moves[MAX_MOVES];
    int count = movegen_legal(pos, moves);
    
    for (int i = 0; i < count; i++) {
        Undo u;
        Position next_pos = *pos;
        
        apply_move(&next_pos, moves[i], &u);
        
        Position refreshed = next_pos;
        nnue_refresh_accumulator(nn, &refreshed);
        
        for (int side = 0; side < 2; side++) {
            for (int k = 0; k < 64; k++) {
                if (refreshed.accum[side][k] != next_pos.accum[side][k]) {
                    char move_str[6];
                    uci_move_to_string(pos, moves[i], move_str, sizeof(move_str));
                    printf("Mismatch after move %s at depth %d, side %d, index %d!\n", move_str, depth, side, k);
                    printf("Refreshed: %d, Incremental: %d\n", refreshed.accum[side][k], next_pos.accum[side][k]);
                    TEST_FAIL_MESSAGE("Accumulator mismatch during recursive search");
                }
            }
        }
        
        nnue_test_incremental_recursive(nn, &next_pos, depth - 1);
    }
}

void test_nnue_incremental_recursive_all_positions(void) {
    const char *fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", // Startpos
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", // Kiwipete
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", // Position 5 (promotions/checks)
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"  // Position 6
    };
    int depths[] = {3, 2, 2, 2};

    int sizes[] = {768, 64, 32, 1};
    NeuralNetwork *nn = nn_init(sizes, 4);
    TEST_ASSERT_NOT_NULL(nn);

    extern NeuralNetwork *eval_nn;
    extern bool use_nn;
    NeuralNetwork *old_eval_nn = eval_nn;
    bool old_use_nn = use_nn;
    eval_nn = nn;
    use_nn = true;

    for (int i = 0; i < 4; i++) {
        Position pos;
        memset(&pos, 0, sizeof(Position));
        TEST_ASSERT_EQUAL_INT(0, fen_parse(fens[i], &pos));

        nnue_refresh_accumulator(nn, &pos);
        nnue_test_incremental_recursive(nn, &pos, depths[i]);
    }

    eval_nn = old_eval_nn;
    use_nn = old_use_nn;
    nn_free(nn);
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
    RUN_TEST(test_nn_feature_extraction);
    RUN_TEST(test_nnue_incremental_correctness);
    RUN_TEST(test_nnue_incremental_recursive_all_positions);

    return UNITY_END();
}

