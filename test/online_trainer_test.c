#include "unity.h"
#include "online_trainer.h"
#include "boards.h"
#include "fen.h"
#include <stdio.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_online_trainer_init_and_free(void)
{
    OnlineTrainer *trainer = online_trainer_init(100, 0.001f, 1e-4f, 0.25f, 400.0f, 32);
    TEST_ASSERT_NOT_NULL(trainer);
    TEST_ASSERT_EQUAL_INT(100, trainer->buffer.capacity);
    TEST_ASSERT_EQUAL_INT(0, trainer->buffer.size);
    TEST_ASSERT_EQUAL_FLOAT(0.001f, trainer->learning_rate);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, trainer->lambda_blend);

    online_trainer_free(trainer);
}

void test_online_trainer_record_and_end_game(void)
{
    bitboard_init();
    OnlineTrainer *trainer = online_trainer_init(50, 0.001f, 1e-4f, 0.25f, 400.0f, 10);
    TEST_ASSERT_NOT_NULL(trainer);

    int sizes[] = {NN_INPUT_SIZE, NN_ACCUM_SIZE, NN_HIDDEN_SIZE, 1};
    NeuralNetwork *nn = nn_init(sizes, 4);
    TEST_ASSERT_NOT_NULL(nn);

    Position pos;
    fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &pos);

    online_trainer_start_game(trainer);
    online_trainer_record_ply(trainer, &pos, 100); // 100 cp for White

    TEST_ASSERT_EQUAL_INT(1, trainer->current_game.num_plies);

    // End game with White victory (1.0f)
    online_trainer_end_game(trainer, nn, 1.0f, NULL);

    TEST_ASSERT_EQUAL_INT(1, trainer->buffer.size);
    TEST_ASSERT_EQUAL_INT(1, trainer->games_processed);
    TEST_ASSERT_GREATER_THAN(0, trainer->total_samples_trained);
    // Verify target is converted to pawn evaluation scale (> 1.0 pawn evaluation for winning target)
    TEST_ASSERT_GREATER_THAN_FLOAT(1.0f, trainer->buffer.samples[0].target);

    nn_free(nn);
    online_trainer_free(trainer);
}

void test_online_trainer_run_selfplay(void)
{
    NeuralNetwork *nn = NULL;
    int res = online_trainer_run_selfplay(&nn, 1, 1, 1, 0.001f, 0.25f, true, NULL);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_NOT_NULL(nn);
    nn_free(nn);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_online_trainer_init_and_free);
    RUN_TEST(test_online_trainer_record_and_end_game);
    RUN_TEST(test_online_trainer_run_selfplay);
    return UNITY_END();
}
