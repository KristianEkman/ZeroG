#ifndef ONLINE_TRAINER_H
#define ONLINE_TRAINER_H

#include "nn.h"
#include "boards.h"
#include <stdbool.h>

#define DEFAULT_REPLAY_BUFFER_CAPACITY 20000
#define GAME_MAX_PLIES 600

typedef struct {
    float inputs[NN_INPUT_SIZE];
    float target;
} OnlineSample;

typedef struct {
    float inputs[NN_INPUT_SIZE];
    int score_cp; // Search evaluation in centipawns
    Color stm;    // Side to move
} GamePlyRecord;

typedef struct {
    OnlineSample *samples;
    int capacity;
    int size;
    int write_index;
} ReplayBuffer;

typedef struct {
    GamePlyRecord plies[GAME_MAX_PLIES];
    int num_plies;
} GameHistory;

typedef struct {
    ReplayBuffer buffer;
    GameHistory current_game;
    float learning_rate;
    float weight_decay;
    float lambda_blend;
    float score_scale; // K constant in Sigmoid: 400.0f
    int batch_size;
    int games_processed;
    int total_samples_trained;
    float last_batch_loss;
    float total_loss_sum;
} OnlineTrainer;

/**
 * Initializes the online trainer with specified parameters.
 */
OnlineTrainer* online_trainer_init(int buffer_capacity, float lr, float weight_decay, float lambda_blend, float score_scale, int batch_size);

/**
 * Safely frees online trainer memory.
 */
void online_trainer_free(OnlineTrainer *trainer);

/**
 * Clears current game ply history for a new game.
 */
void online_trainer_start_game(OnlineTrainer *trainer);

/**
 * Records a position and its search evaluation (depth X) during self-play.
 */
void online_trainer_record_ply(OnlineTrainer *trainer, const Position *pos, int search_score_cp);

/**
 * Concludes a game, calculates blended targets y = (1 - lambda) * Sigmoid(v / K) + lambda * Z_outcome,
 * pushes samples to the replay buffer, and performs mini-batch training steps.
 * 
 * @param trainer Pointer to OnlineTrainer.
 * @param nn Pointer to NeuralNetwork.
 * @param result_white 1.0f for White win, 0.5f for draw, 0.0f for Black win.
 * @param weights_save_path Optional file path to save updated weights (can be NULL).
 */
void online_trainer_end_game(OnlineTrainer *trainer, NeuralNetwork *nn, float result_white, const char *weights_save_path);

/**
 * Runs self-play training loop for a given number of games and search depth.
 * 
 * @param nn_io Pointer to NeuralNetwork pointer (or NULL to initialize a new random network). Updated if reset/reallocated.
 * @param num_games Number of games to play.
 * @param depth Search depth for self-play evaluations.
 * @param threads Number of search threads (e.g. 1, 4, 8).
 * @param lr Learning rate for AdamW trainer.
 * @param lambda_blend Blend factor between search score and outcome (e.g. 0.25).
 * @param reset_network If true, initializes a fresh random network.
 * @param weights_save_path File path to save trained weights.
 * @return 0 on success, non-zero on failure.
 */
int online_trainer_run_selfplay(NeuralNetwork **nn_io, int num_games, int depth, int threads, float lr, float lambda_blend, bool reset_network, const char *weights_save_path);

#endif // ONLINE_TRAINER_H
