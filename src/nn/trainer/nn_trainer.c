#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

#include "boards.h"
#include "fen.h"
#include "nn.h"
#include "movegen.h"

typedef struct {
    float inputs[768];
    float target;
} TrainingSample;

// Parses an EPD line, populating position and target score (normalized to pawn units)
int parse_epd_line(const char *line, Position *pos, float *target) {
    char line_copy[1024];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';
    
    // Find the score substring
    char *score_ptr = strstr(line_copy, "score ");
    if (!score_ptr) {
        return -1; // No score opcode
    }
    
    // Skip positions containing mate scores
    if (strstr(score_ptr, "mate")) {
        return -2;
    }
    
    // Parse score value (centipawns)
    int score_val = atoi(score_ptr + 6);
    
    // Skip extreme positions
    if (score_val >= 1000 || score_val <= -1000) {
        return -3;
    }
    
    // Target is scaled from centipawns to pawn units (e.g. 150 -> 1.5)
    *target = (float)score_val / 100.0f;
    
    // Truncate string at "score" to isolate the FEN part
    *score_ptr = '\0';
    
    // Trim trailing spaces and semicolons from the FEN
    char *end = score_ptr - 1;
    while (end >= line_copy && (*end == ' ' || *end == ';' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    
    // Parse the FEN into Position
    if (fen_parse(line_copy, pos) != 0) {
        return -4;
    }
    
    return 0;
}

// Shuffles dataset using Fisher-Yates algorithm
void shuffle_dataset(TrainingSample *dataset, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        TrainingSample temp = dataset[i];
        dataset[i] = dataset[j];
        dataset[j] = temp;
    }
}

void print_help(const char *prog_name) {
    printf("ChessAI2027 Neural Network Trainer\n");
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -i, --input <file>     Input EPD file (default: quiet_training_positions_evaluated.epd)\n");
    printf("  -o, --output <file>    Output weights file (default: nn_weights.bin)\n");
    printf("  -w, --weights <file>   Initial weights file to continue training from (optional)\n");
    printf("  -e, --epochs <num>     Number of training epochs (default: 30)\n");
    printf("  -l, --lr <value>       Initial learning rate (default: 0.01)\n");
    printf("  -v, --val-split <val>  Validation split ratio (default: 0.1)\n");
    printf("  -h, --help             Display this help and exit\n");
}

int main(int argc, char **argv) {
    // 1. Parse command line arguments
    const char *input_path = "quiet_training_positions_evaluated.epd";
    const char *output_path = "nn_weights.bin";
    const char *weights_path = NULL;
    int epochs = 30;
    float initial_lr = 0.01f;
    float val_split = 0.1f;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0) {
            if (i + 1 < argc) input_path = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_path = argv[++i];
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--weights") == 0) {
            if (i + 1 < argc) weights_path = argv[++i];
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--epochs") == 0) {
            if (i + 1 < argc) epochs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--lr") == 0) {
            if (i + 1 < argc) initial_lr = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--val-split") == 0) {
            if (i + 1 < argc) val_split = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s. Use -h or --help for details.\n", argv[i]);
            return 1;
        }
    }
    
    // 2. Initialize engine components (essential for FEN parsing)
    printf("Initializing engine bitboards...\n");
    bitboard_init();
    
    // 3. Count valid positions in EPD file
    printf("Reading EPD file: %s\n", input_path);
    FILE *f = fopen(input_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open EPD file '%s'\n", input_path);
        return 1;
    }
    
    char line[1024];
    int total_lines = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != '\n' && line[0] != '\r' && line[0] != '#') {
            total_lines++;
        }
    }
    rewind(f);
    
    if (total_lines == 0) {
        fprintf(stderr, "Error: No lines to parse in EPD file '%s'\n", input_path);
        fclose(f);
        return 1;
    }
    printf("Found %d raw lines in EPD.\n", total_lines);
    
    // Allocate dataset memory
    TrainingSample *dataset = malloc(total_lines * sizeof(TrainingSample));
    if (!dataset) {
        fprintf(stderr, "Error: Out of memory allocating dataset buffer.\n");
        fclose(f);
        return 1;
    }
    
    // 4. Parse positions and extract features
    int parsed_count = 0;
    int skipped_count = 0;
    Position temp_pos;
    float target = 0.0f;
    
    printf("Parsing positions and extracting features...\n");
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') {
            continue;
        }
        
        int res = parse_epd_line(line, &temp_pos, &target);
        if (res == 0) {
            // Extract feature mapping
            nn_extract_features(&temp_pos, dataset[parsed_count].inputs);
            dataset[parsed_count].target = target;
            parsed_count++;
        } else {
            skipped_count++;
        }
    }
    fclose(f);
    
    printf("Successfully parsed: %d | Skipped (mate/invalid/extreme): %d\n", parsed_count, skipped_count);
    if (parsed_count == 0) {
        fprintf(stderr, "Error: Zero valid training samples extracted.\n");
        free(dataset);
        return 1;
    }
    
    // 5. Shuffle and split dataset
    printf("Shuffling and splitting dataset (Validation Split: %.1f%%)...\n", val_split * 100.0f);
    srand(42); // Seed with fixed number for reproducibility
    shuffle_dataset(dataset, parsed_count);
    
    int val_size = (int)(parsed_count * val_split);
    int train_size = parsed_count - val_size;
    
    TrainingSample *train_set = dataset;
    TrainingSample *val_set = dataset + train_size;
    
    printf("Train size: %d | Validation size: %d\n", train_size, val_size);
    
    // 6. Initialize Neural Network (768 -> 64 -> 32 -> 1)
    int sizes[] = {768, 64, 32, 1};
    int num_layers = sizeof(sizes) / sizeof(sizes[0]);
    printf("Initializing neural network with layout: {768, 64, 32, 1}...\n");
    NeuralNetwork *nn = nn_init(sizes, num_layers);
    if (!nn) {
        fprintf(stderr, "Error: Neural network initialization failed.\n");
        free(dataset);
        return 1;
    }
    
    if (weights_path) {
        printf("Loading existing weights from: %s\n", weights_path);
        if (!nn_load(nn, weights_path)) {
            fprintf(stderr, "Error: Failed to load weights from '%s'\n", weights_path);
            nn_free(nn);
            free(dataset);
            return 1;
        }
        printf("Successfully loaded weights. Continuing training...\n");
    }
    
    // 7. Training loop
    float current_lr = initial_lr;
    printf("\nStarting training (%d epochs, initial lr = %.5f)...\n", epochs, current_lr);
    printf("----------------------------------------------------------\n");
    
    clock_t start_time = clock();
    
    for (int epoch = 1; epoch <= epochs; epoch++) {
        // Reshuffle train set each epoch
        shuffle_dataset(train_set, train_size);
        
        float total_train_loss = 0.0f;
        for (int i = 0; i < train_size; i++) {
            float loss = nn_train_step(nn, train_set[i].inputs, train_set[i].target, current_lr);
            total_train_loss += loss;
        }
        float avg_train_loss = total_train_loss / train_size;
        
        // Evaluate validation set
        float total_val_loss = 0.0f;
        for (int i = 0; i < val_size; i++) {
            float out = nn_forward(nn, val_set[i].inputs);
            float diff = out - val_set[i].target;
            total_val_loss += 0.5f * diff * diff;
        }
        float avg_val_loss = (val_size > 0) ? (total_val_loss / val_size) : 0.0f;
        
        printf("Epoch %2d/%2d | Train Loss: %.6f | Val Loss: %.6f | LR: %.6f\n",
               epoch, epochs, avg_train_loss, avg_val_loss, current_lr);
        
        // Learning rate decay schedule: half the learning rate every 10 epochs
        if (epoch % 10 == 0) {
            current_lr *= 0.5f;
        }
    }
    
    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("----------------------------------------------------------\n");
    printf("Training completed in %.2f seconds.\n", elapsed_time);
    
    // 8. Save final weights
    printf("Saving trained weights to: %s\n", output_path);
    if (nn_save(nn, output_path)) {
        printf("Successfully saved weights!\n");
    } else {
        fprintf(stderr, "Error: Failed to save weights to '%s'\n", output_path);
    }
    
    // 9. Clean up
    nn_free(nn);
    free(dataset);
    printf("Trainer done.\n");
    return 0;
}
