#include "online_trainer.h"
#include "eval.h"
#include "fen.h"
#include "movegen.h"
#include "search.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float sigmoid_win_prob(float eval_cp, float scale) {
  if (scale <= 0.0f)
    scale = 400.0f;
  return 1.0f / (1.0f + powf(10.0f, -eval_cp / scale));
}

static float probability_to_pawns(float p, float scale_cp) {
  const float eps = 0.001f;

  if (p < eps) {
    p = eps;
  } else if (p > 1.0f - eps) {
    p = 1.0f - eps;
  }

  float eval_cp = scale_cp * log10f(p / (1.0f - p));
  return eval_cp / 100.0f;
}

OnlineTrainer *online_trainer_init(int buffer_capacity, float lr,
                                   float weight_decay, float lambda_blend,
                                   float score_scale, int batch_size) {
  OnlineTrainer *trainer = (OnlineTrainer *)calloc(1, sizeof(OnlineTrainer));
  if (!trainer)
    return NULL;

  int capacity =
      (buffer_capacity > 0) ? buffer_capacity : DEFAULT_REPLAY_BUFFER_CAPACITY;
  trainer->buffer.samples =
      (OnlineSample *)malloc(capacity * sizeof(OnlineSample));
  if (!trainer->buffer.samples) {
    free(trainer);
    return NULL;
  }
  trainer->buffer.capacity = capacity;
  trainer->buffer.size = 0;
  trainer->buffer.write_index = 0;

  trainer->learning_rate = (lr > 0.0f) ? lr : 0.0005f;
  trainer->weight_decay = (weight_decay >= 0.0f) ? weight_decay : 1e-4f;
  trainer->lambda_blend =
      (lambda_blend >= 0.0f && lambda_blend <= 1.0f) ? lambda_blend : 0.25f;
  trainer->score_scale = (score_scale > 0.0f) ? score_scale : 400.0f;
  trainer->batch_size = (batch_size > 0) ? batch_size : 64;
  trainer->games_processed = 0;
  trainer->total_samples_trained = 0;

  return trainer;
}

void online_trainer_free(OnlineTrainer *trainer) {
  if (!trainer)
    return;
  if (trainer->buffer.samples) {
    free(trainer->buffer.samples);
  }
  free(trainer);
}

void online_trainer_start_game(OnlineTrainer *trainer) {
  if (!trainer)
    return;
  trainer->current_game.num_plies = 0;
}

void online_trainer_record_ply(OnlineTrainer *trainer, const Position *pos,
                               int search_score_cp) {
  if (!trainer || !pos)
    return;
  if (trainer->current_game.num_plies >= GAME_MAX_PLIES)
    return;

  int idx = trainer->current_game.num_plies;
  GamePlyRecord *rec = &trainer->current_game.plies[idx];

  rec->pos = *pos;
  rec->score_cp = search_score_cp;
  rec->stm = pos->sideToMove;

  trainer->current_game.num_plies++;
}

void online_trainer_end_game(OnlineTrainer *trainer, NeuralNetwork *nn,
                             float result_white,
                             const char *weights_save_path) {
  if (!trainer || !nn)
    return;

  int num_plies = trainer->current_game.num_plies;
  if (num_plies == 0)
    return;

  // 1. Process game plies and add blended targets to replay buffer
  for (int i = 0; i < num_plies; i++) {
    GamePlyRecord *rec = &trainer->current_game.plies[i];

    float p_search =
        sigmoid_win_prob((float)rec->score_cp, trainer->score_scale);
    float z_outcome =
        (rec->stm == WHITE) ? result_white : (1.0f - result_white);

    float p_target = (1.0f - trainer->lambda_blend) * p_search +
                     trainer->lambda_blend * z_outcome;
    float target = probability_to_pawns(p_target, trainer->score_scale);

    int write_idx = trainer->buffer.write_index;
    trainer->buffer.samples[write_idx].pos = position_to_compact(&rec->pos);
    trainer->buffer.samples[write_idx].target = target;

    trainer->buffer.write_index = (write_idx + 1) % trainer->buffer.capacity;
    if (trainer->buffer.size < trainer->buffer.capacity) {
      trainer->buffer.size++;
    }
  }

  trainer->games_processed++;

  // 2. Perform parallel mini-batch training steps from replay buffer
  if (trainer->buffer.size > 0) {
    int steps = (trainer->batch_size < trainer->buffer.size)
                    ? trainer->batch_size
                    : trainer->buffer.size;

    TrainingSample *batch_samples = (TrainingSample *)malloc(steps * sizeof(TrainingSample));
    if (batch_samples) {
      for (int step = 0; step < steps; step++) {
        int rand_idx = rand() % trainer->buffer.size;
        OnlineSample *sample = &trainer->buffer.samples[rand_idx];
        batch_samples[step].pos = sample->pos;
        batch_samples[step].target = sample->target;
      }

      NNBatchTrainer *batch_trainer = nn_batch_trainer_init(nn, 4);
      if (batch_trainer) {
        float batch_loss = nn_train_batch_parallel(batch_trainer, nn, batch_samples, steps,
                                                   trainer->learning_rate, trainer->weight_decay);
        trainer->last_batch_loss = batch_loss / steps;
        trainer->total_samples_trained += steps;
        trainer->total_loss_sum += trainer->last_batch_loss;
        nn_batch_trainer_free(batch_trainer);
      } else {
        float batch_loss_sum = 0.0f;
        for (int step = 0; step < steps; step++) {
          Position full_pos;
          compact_to_position(&batch_samples[step].pos, &full_pos);
          float step_loss = nn_train_step_pos(nn, &full_pos, batch_samples[step].target,
                                               trainer->learning_rate, trainer->weight_decay);
          batch_loss_sum += step_loss;
          trainer->total_samples_trained++;
        }
        trainer->last_batch_loss = (steps > 0) ? (batch_loss_sum / steps) : 0.0f;
        trainer->total_loss_sum += trainer->last_batch_loss;
      }
      free(batch_samples);
    }

    // 3. Re-quantize weights for fast search evaluation
    nn_quantize(nn);
  }

  // 4. Save updated weights if path provided
  if (weights_save_path && strlen(weights_save_path) > 0) {
    nn_save(nn, weights_save_path);
  }

  // Reset game buffer
  trainer->current_game.num_plies = 0;
}

static int load_epd_openings(const char *filename, char fens[][256],
                             int max_fens) {
  FILE *f = fopen(filename, "r");
  if (!f)
    return 0;
  int count = 0;
  char line[512];
  while (fgets(line, sizeof(line), f) && count < max_fens) {
    char *p = line;
    while (*p) {
      if (*p == '\r' || *p == '\n') {
        *p = '\0';
        break;
      }
      p++;
    }
    // Truncate line at EPD operation separator (e.g. ';') if present
    char *semi = strchr(line, ';');
    if (semi)
      *semi = '\0';

    // Trim trailing whitespace
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t')) {
      line[--len] = '\0';
    }

    if (len > 10) {
      strncpy(fens[count], line, 255);
      fens[count][255] = '\0';
      count++;
    }
  }
  fclose(f);
  return count;
}

int online_trainer_run_selfplay(NeuralNetwork **nn_io, int num_games, int depth,
                                int threads, float lr, float lambda_blend,
                                bool reset_network,
                                const char *weights_save_path) {
  if (num_games <= 0)
    num_games = 10;
  if (depth <= 0)
    depth = 7;
  int num_threads = (threads > 0) ? threads : 1;
  search_set_threads(num_threads);

  const char *save_path = (weights_save_path && strlen(weights_save_path) > 0)
                              ? weights_save_path
                              : "nn_weights.bin";

  // 1. Initialize or reset network if requested
  NeuralNetwork *target_nn = (nn_io && *nn_io) ? *nn_io : NULL;
  if (reset_network || target_nn == NULL) {
    // Free existing network before replacing
    if (target_nn) {
      nn_free(target_nn);
      target_nn = NULL;
    }
    int sizes[] = {NN_INPUT_SIZE, NN_ACCUM_SIZE, NN_HIDDEN_SIZE, 1};
    target_nn = nn_init(sizes, 4);
    if (!target_nn) {
      fprintf(stderr, "Error: Failed to initialize neural network.\n");
      return 1;
    }
    if (nn_io) {
      *nn_io = target_nn;
    }
    printf(
        "Initialized fresh random neural network (780 -> 192 -> 32 -> 1).\n");
  }

  // Ensure weights are quantized before starting self-play
  nn_quantize(target_nn);

  // Set global evaluation pointers
  eval_nn = target_nn;
  use_nn = true;

  // Load opening positions for game diversity
  static char epd_fens[30000][256];
  const char *epd_files[] = {"training/data/top-openings.epd", "training/data/quiet_training_positions.epd", "training/data/selfplay_positions.epd", "games/top-openings.epd", "quiet_training_positions.epd", "selfplay_positions.epd"};
  int epd_count = 0;
  const char *loaded_file = NULL;
  for (size_t i = 0; i < sizeof(epd_files) / sizeof(epd_files[0]); i++) {
    epd_count = load_epd_openings(epd_files[i], epd_fens, 30000);
    if (epd_count > 0) {
      loaded_file = epd_files[i];
      break;
    }
  }

  if (epd_count == 0) {
    strcpy(epd_fens[0], "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    epd_count = 1;
    loaded_file = "startpos (default)";
  }

  // 2. Initialize Online Trainer
  OnlineTrainer *trainer = online_trainer_init(
      DEFAULT_REPLAY_BUFFER_CAPACITY, lr, 1e-4f, lambda_blend, 400.0f, 64);
  if (!trainer) {
    fprintf(stderr, "Error: Failed to initialize online trainer.\n");
    return 1;
  }

  printf("Starting Online NN Self-Play Training:\n");
  printf("  Games: %d\n", num_games);
  printf("  Depth: %d\n", depth);
  printf("  Threads: %d\n", num_threads);
  printf("  Learning Rate: %.5f\n", trainer->learning_rate);
  printf("  Lambda (Outcome Blend): %.2f\n", trainer->lambda_blend);
  if (epd_count > 0) {
    printf("  Opening Diversity: %d positions loaded from %s\n", epd_count,
           loaded_file);
  }
  printf("  Saving to: %s\n", save_path);
  printf("--------------------------------------------------\n");

  int white_wins = 0, black_wins = 0, draws = 0;

  for (int g = 1; g <= num_games; g++) {
    Position pos;
    const char *start_fen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    if (epd_count > 0) {
      start_fen = epd_fens[rand() % epd_count];
    }

    if (fen_parse(start_fen, &pos) != 0) {
      fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                &pos);
    }

    online_trainer_start_game(trainer);

    SearchLimits limits;
    memset(&limits, 0, sizeof(SearchLimits));
    limits.depth = depth;
    limits.history_count = 0;
    limits.history_hashes[limits.history_count++] = pos.hashKey;

    int ply_count = 0;
    float result_white = 0.5f; // Default draw

    while (ply_count < 200) {
      Move legal_moves[MAX_MOVES];
      int num_legal = movegen_legal(&pos, legal_moves);

      if (num_legal == 0) {
        // Checkmate or Stalemate
        if (is_square_attacked(&pos, pos.kingSq[COLOR_IDX(pos.sideToMove)],
                               OPPOSITE(pos.sideToMove))) {
          // Side to move is checkmated
          result_white = (pos.sideToMove == WHITE) ? 0.0f : 1.0f;
          if (result_white == 1.0f)
            white_wins++;
          else
            black_wins++;
        } else {
          // Stalemate
          result_white = 0.5f;
          draws++;
        }
        break;
      }

      if (pos.fiftyMoveCounter >= 100) {
        // 50-move draw rule
        result_white = 0.5f;
        draws++;
        break;
      }

      // Execute search at specified depth
      SearchResult res;
      memset(&res, 0, sizeof(SearchResult));

      search_reset_stop_request();
      if (search_best_move_with_limits(&pos, &limits, &res) != 0 ||
          !res.has_legal_move) {
        // Fallback to first legal move
        res.best_move = legal_moves[0];
        int white_score = evaluate(&pos);
        res.score = (pos.sideToMove == WHITE) ? white_score : -white_score;
      }

      // Choose move (with 20% random exploration on first 4 plies)
      Move chosen_move = res.best_move;
      if (ply_count < 4 && (rand() % 100) < 20) {
        chosen_move = legal_moves[rand() % num_legal];
      }

      // Record ply features & search eval
      online_trainer_record_ply(trainer, &pos, res.score);

      // Apply move
      Undo u;
      apply_move(&pos, chosen_move, &u);
      if (limits.history_count < 1024) {
        limits.history_hashes[limits.history_count++] = pos.hashKey;
      }
      ply_count++;

      // Repetition adjudication
      int rep_count = 0;
      for (int h = 0; h < limits.history_count - 1; h++) {
        if (limits.history_hashes[h] == pos.hashKey) {
          rep_count++;
        }
      }
      if (rep_count >= 2) {
        result_white = 0.5f;
        draws++;
        break;
      }
    }

    if (ply_count >= 200) {
      draws++; // Max plies reached -> draw
    }

    // End game and train
    online_trainer_end_game(trainer, target_nn, result_white, save_path);

    const char *result_str = (result_white == 1.0f)
                                 ? "1-0"
                                 : ((result_white == 0.0f) ? "0-1" : "1/2-1/2");
    printf("Game %d/%d completed in %d plies [%s] (Buffer: %d, Batch Loss: "
           "%.5f)\n",
           g, num_games, ply_count, result_str, trainer->buffer.size,
           trainer->last_batch_loss);
    fflush(stdout);
  }

  float avg_overall_loss =
      (trainer->games_processed > 0)
          ? (trainer->total_loss_sum / trainer->games_processed)
          : 0.0f;

  printf("--------------------------------------------------\n");
  printf("Training Completed! Score: +%d -%d =%d (Avg Loss: %.5f)\n",
         white_wins, black_wins, draws, avg_overall_loss);
  printf("Saved updated network weights to %s\n", save_path);

  online_trainer_free(trainer);
  return 0;
}
