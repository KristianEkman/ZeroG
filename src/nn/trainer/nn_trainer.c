#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static int get_logical_cpu_count(void) {
#if defined(_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    return cpus > 0 ? (int)cpus : 4;
#elif defined(_SC_NPROCESSORS_CONF)
    long cpus = sysconf(_SC_NPROCESSORS_CONF);
    return cpus > 0 ? (int)cpus : 4;
#else
    return 4;
#endif
}

#include "boards.h"
#include "fen.h"
#include "nn.h"
#include "movegen.h"

typedef enum {
    PERSPECTIVE_STM,
    PERSPECTIVE_WHITE
} ScorePerspective;

// Shuffles dataset using Fisher-Yates algorithm
void shuffle_dataset(TrainingSample *dataset, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        TrainingSample temp = dataset[i];
        dataset[i] = dataset[j];
        dataset[j] = temp;
    }
}

// Single-pass EPD file loader with dynamic memory reallocation
TrainingSample* load_epd_file(const char *path, int *out_count, ScorePerspective perspective) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return NULL;
    }

    int capacity = 1024;
    int count = 0;
    TrainingSample *samples = malloc(capacity * sizeof(TrainingSample));
    if (!samples) {
        fclose(f);
        return NULL;
    }

    char line[1024];
    Position temp_pos;
    float target = 0.0f;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') {
            continue;
        }
        int res = parse_epd_line(line, &temp_pos, &target);
        if (res == 0) {
            if (perspective == PERSPECTIVE_WHITE && temp_pos.sideToMove == BLACK) {
                target = -target;
            }
            if (count >= capacity) {
                capacity *= 2;
                TrainingSample *new_samples = realloc(samples, capacity * sizeof(TrainingSample));
                if (!new_samples) {
                    fprintf(stderr, "Error: Out of memory reallocating samples for '%s'.\n", path);
                    free(samples);
                    fclose(f);
                    return NULL;
                }
                samples = new_samples;
            }
            samples[count].target = target;
            samples[count].pos = position_to_compact(&temp_pos);
            count++;
        }
    }
    fclose(f);

    if (count == 0) {
        free(samples);
        samples = NULL;
    } else {
        TrainingSample *shrunk = realloc(samples, count * sizeof(TrainingSample));
        if (shrunk) {
            samples = shrunk;
        }
    }

    *out_count = count;
    return samples;
}

static double get_time_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

void print_help(const char *prog_name) {
    printf("ZeroG Neural Network Trainer (Multi-Threaded Parallel Mini-Batch)\n");
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -i, --input <file>     Input EPD file (default: quiet_training_positions_evaluated.epd)\n");
    printf("  -o, --output <file>    Output weights file (default: nn_weights.bin)\n");
    printf("  -w, --weights <file>   Initial weights file to continue training from (optional)\n");
    printf("  -e, --epochs <num>     Number of training epochs (default: 30)\n");
    printf("  -b, --batch-size <num> Batch size for training (default: 4096)\n");
    printf("  -t, --threads <num>    Number of worker threads (default: 16 max)\n");
    printf("  -l, --lr <value>       Initial learning rate (default: 0.001)\n");
    printf("  -v, --val-split <val>  Validation split ratio (default: 0.1)\n");
    printf("  --val-file <file>      Static validation EPD file (disables validation split if provided)\n");
    printf("  -d, --wd <value>       Weight decay coefficient (default: 1e-4)\n");
    printf("  -p, --score-perspective <persp> Score perspective: 'stm' or 'white' (default: stm)\n");
    printf("  -h, --help             Display this help and exit\n");
}

int main(int argc, char **argv) {
    // 1. Parse command line arguments
    const char *input_path = "quiet_training_positions_evaluated.epd";
    const char *output_path = "nn_weights.bin";
    const char *weights_path = NULL;
    const char *val_file_path = NULL;
    int epochs = 30;
    int batch_size = 4096;
    long sys_cpus = get_logical_cpu_count();
    int default_threads = (sys_cpus > 0 && sys_cpus <= 16) ? (int)sys_cpus : 16;
    int num_threads = default_threads;
    float initial_lr = 0.001f;
    float val_split = 0.1f;
    float weight_decay = 1e-4f;
    ScorePerspective perspective = PERSPECTIVE_STM;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0) {
            if (i + 1 < argc) input_path = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_path = argv[++i];
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--weights") == 0) {
            if (i + 1 < argc) weights_path = argv[++i];
        } else if (strcmp(argv[i], "--val-file") == 0 || strcmp(argv[i], "--validation") == 0) {
            if (i + 1 < argc) val_file_path = argv[++i];
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--epochs") == 0) {
            if (i + 1 < argc) epochs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--batch-size") == 0) {
            if (i + 1 < argc) batch_size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) num_threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--lr") == 0) {
            if (i + 1 < argc) initial_lr = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--val-split") == 0) {
            if (i + 1 < argc) val_split = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--wd") == 0) {
            if (i + 1 < argc) weight_decay = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--score-perspective") == 0) {
            if (i + 1 < argc) {
                const char *persp = argv[++i];
                if (strcmp(persp, "stm") == 0) {
                    perspective = PERSPECTIVE_STM;
                } else if (strcmp(persp, "white") == 0) {
                    perspective = PERSPECTIVE_WHITE;
                } else {
                    fprintf(stderr, "Invalid score perspective '%s'. Use 'stm' or 'white'.\n", persp);
                    return 1;
                }
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s. Use -h or --help for details.\n", argv[i]);
            return 1;
        }
    }
    
    if (epochs <= 0 || batch_size <= 0 || num_threads <= 0 ||
        !isfinite(initial_lr) || initial_lr <= 0.0f ||
        !isfinite(val_split) || val_split <= 0.0f || val_split >= 1.0f ||
        !isfinite(weight_decay) || weight_decay < 0.0f) {
        fprintf(stderr, "Invalid training parameters\n");
        return 1;
    }
    
    if (num_threads > 16) num_threads = 16;
    
    // 2. Initialize engine components
    printf("Initializing engine bitboards...\n");
    bitboard_init();
    
    // 3. Detect if input_path is a list file (ends with .txt or .lst)
    bool is_list_file = false;
    size_t input_len = strlen(input_path);
    if (input_len > 4) {
        const char *ext = input_path + input_len - 4;
        if (strcasecmp(ext, ".txt") == 0 || strcasecmp(ext, ".lst") == 0) {
            is_list_file = true;
        }
    }

    char **file_paths = NULL;
    int num_files = 0;

    if (is_list_file) {
        printf("Reading EPD file list: %s\n", input_path);
        FILE *lf = fopen(input_path, "r");
        if (!lf) {
            fprintf(stderr, "Error: Could not open list file '%s'\n", input_path);
            return 1;
        }
        char path_line[1024];
        while (fgets(path_line, sizeof(path_line), lf)) {
            // Trim trailing spaces and newlines
            size_t len = strlen(path_line);
            while (len > 0 && isspace((unsigned char)path_line[len - 1])) {
                path_line[len - 1] = '\0';
                len--;
            }
            // Skip empty lines and comments
            if (len == 0 || path_line[0] == '#') {
                continue;
            }
            file_paths = realloc(file_paths, (num_files + 1) * sizeof(char *));
            file_paths[num_files] = strdup(path_line);
            num_files++;
        }
        fclose(lf);
        if (num_files == 0) {
            fprintf(stderr, "Error: No EPD files listed in '%s'\n", input_path);
            return 1;
        }
        printf("Found %d EPD files to process.\n", num_files);
    } else {
        file_paths = malloc(sizeof(char *));
        file_paths[0] = strdup(input_path);
        num_files = 1;
    }

    // 4. Initialize Neural Network
    int sizes[] = {NN_INPUT_SIZE, NN_ACCUM_SIZE, NN_HIDDEN_SIZE, 1};
    int num_layers = sizeof(sizes) / sizeof(sizes[0]);
    printf("Initializing neural network with layout: {%d, %d, %d, 1}...\n", NN_INPUT_SIZE, NN_ACCUM_SIZE, NN_HIDDEN_SIZE);
    NeuralNetwork *nn = nn_init(sizes, num_layers);
    if (!nn) {
        fprintf(stderr, "Error: Neural network initialization failed.\n");
        for (int i = 0; i < num_files; i++) free(file_paths[i]);
        free(file_paths);
        return 1;
    }

    if (weights_path) {
        printf("Loading existing weights from: %s\n", weights_path);
        if (!nn_load(nn, weights_path)) {
            fprintf(stderr, "Error: Failed to load weights from '%s'\n", weights_path);
            nn_free(nn);
            for (int i = 0; i < num_files; i++) free(file_paths[i]);
            free(file_paths);
            return 1;
        }
        printf("Successfully loaded weights. Continuing training...\n");
    }

    // Initialize Batch Trainer
    NNBatchTrainer *batch_trainer = nn_batch_trainer_init(nn, num_threads);
    if (!batch_trainer) {
        fprintf(stderr, "Error: Failed to initialize batch trainer.\n");
        nn_free(nn);
        for (int i = 0; i < num_files; i++) free(file_paths[i]);
        free(file_paths);
        return 1;
    }

    // 5. Optionally load static validation dataset
    TrainingSample *global_val_set = NULL;
    int global_val_size = 0;
    if (val_file_path) {
        printf("Loading static validation dataset from: %s\n", val_file_path);
        global_val_set = load_epd_file(val_file_path, &global_val_size, perspective);
        if (!global_val_set || global_val_size == 0) {
            fprintf(stderr, "Error: Failed to load static validation dataset from '%s'\n", val_file_path);
            nn_batch_trainer_free(batch_trainer);
            nn_free(nn);
            for (int i = 0; i < num_files; i++) free(file_paths[i]);
            free(file_paths);
            return 1;
        }
        printf("Loaded %d validation samples.\n", global_val_size);
    }

    // 6. Training loop across files
    float current_lr = initial_lr;
    float best_val_loss = 1e30f;
    int best_epoch = 0;
    int global_epoch = 0;
    double start_time = get_time_sec();

    printf("\nStarting training (%d epochs per file, initial lr = %.5f, wd = %.1e)...\n",
           epochs, current_lr, weight_decay);
    printf("------------------------------------------------------------------------------------\n");

    for (int file_idx = 0; file_idx < num_files; file_idx++) {
        const char *cur_file = file_paths[file_idx];
        printf("\n[File %d/%d] Reading: %s\n", file_idx + 1, num_files, cur_file);

        int parsed_count = 0;
        TrainingSample *dataset = load_epd_file(cur_file, &parsed_count, perspective);
        if (!dataset) {
            fprintf(stderr, "Warning: Could not read or parse EPD file '%s'. Skipping.\n", cur_file);
            continue;
        }
        printf("Successfully parsed: %d positions\n", parsed_count);

        TrainingSample *train_set = NULL;
        TrainingSample *val_set = NULL;
        int train_size = 0;
        int val_size = 0;

        if (global_val_set) {
            train_size = parsed_count;
            train_set = dataset;
            val_size = global_val_size;
            val_set = global_val_set;
            // Shuffle whole training set
            srand(42 + file_idx); // Vary seed per file for diversity
            shuffle_dataset(train_set, train_size);
        } else {
            // Fallback to local train/val split
            srand(42 + file_idx); // Vary seed per file for diversity
            shuffle_dataset(dataset, parsed_count);

            val_size = (int)(parsed_count * val_split);
            train_size = parsed_count - val_size;

            if (train_size < 1 || val_size < 1) {
                fprintf(stderr, "Dataset is too small for the selected validation split. Skipping.\n");
                free(dataset);
                continue;
            }

            train_set = dataset;
            val_set = dataset + train_size;
        }

        printf("Train size: %d | Validation size: %d\n", train_size, val_size);
        printf("Batch size: %d | Threads: %d\n", batch_size, num_threads);

        for (int epoch = 1; epoch <= epochs; epoch++) {
            global_epoch++;
            double epoch_start = get_time_sec();
            shuffle_dataset(train_set, train_size);

            float total_train_loss = 0.0f;
            for (int i = 0; i < train_size; i += batch_size) {
                int cur_batch_size = (i + batch_size <= train_size) ? batch_size : (train_size - i);
                float b_loss = nn_train_batch_parallel(batch_trainer, nn, &train_set[i], cur_batch_size, current_lr, weight_decay);
                if (!isfinite(b_loss)) {
                    fprintf(stderr, "Error: Training failed at file %d, epoch %d, sample %d\n", file_idx + 1, epoch, i);
                    free(dataset);
                    if (global_val_set) free(global_val_set);
                    nn_batch_trainer_free(batch_trainer);
                    nn_free(nn);
                    for (int k = 0; k < num_files; k++) free(file_paths[k]);
                    free(file_paths);
                    return 1;
                }
                total_train_loss += b_loss;
            }
            float avg_train_loss = total_train_loss / train_size;

            // Quantize network weights
            nn_quantize(nn);

            // Multi-threaded validation loss evaluation (both float & quantized)
            float avg_quant_val_loss = 0.0f;
            float avg_val_loss = nn_evaluate_batch_parallel(batch_trainer, nn, val_set, val_size, &avg_quant_val_loss);

            double epoch_time = get_time_sec() - epoch_start;
            double kpos_per_sec = (double)train_size / (epoch_time * 1000.0);

            int is_best = (avg_quant_val_loss < best_val_loss);
            if (is_best) {
                best_val_loss = avg_quant_val_loss;
                best_epoch = global_epoch;
                if (!nn_save(nn, output_path)) {
                    fprintf(stderr, "Error: Failed to save model to '%s'\n", output_path);
                    free(dataset);
                    if (global_val_set) free(global_val_set);
                    nn_batch_trainer_free(batch_trainer);
                    nn_free(nn);
                    for (int k = 0; k < num_files; k++) free(file_paths[k]);
                    free(file_paths);
                    return 1;
                }
            }

            printf("Epoch %2d (Global %2d) | Train Loss: %.6f | Float Val Loss: %.6f | Quant Val Loss: %.6f | LR: %.6f | Time: %.2fs (%.1fk pos/s)%s\n",
                   epoch, global_epoch, avg_train_loss, avg_val_loss, avg_quant_val_loss, current_lr,
                   epoch_time, kpos_per_sec, is_best ? " *" : "");

            int decay_step = (num_files * epochs) / 5;
            if (decay_step < 10) decay_step = 10;
            if (global_epoch % decay_step == 0) {
                current_lr *= 0.5f;
            }
        }

        free(dataset);
    }

    double elapsed_time = get_time_sec() - start_time;
    printf("------------------------------------------------------------------------------------\n");
    printf("Training completed in %.2f seconds (processed %d files).\n", elapsed_time, num_files);
    printf("Best validation loss: %.6f at global epoch %d (saved to %s)\n",
           best_val_loss, best_epoch, output_path);

    // Clean up
    if (global_val_set) {
        free(global_val_set);
    }
    nn_batch_trainer_free(batch_trainer);
    nn_free(nn);
    for (int i = 0; i < num_files; i++) {
        free(file_paths[i]);
    }
    free(file_paths);
    printf("Trainer done.\n");
    return 0;
}
