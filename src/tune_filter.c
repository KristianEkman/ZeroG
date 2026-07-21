#include "tune_filter.h"
#include "boards.h"
#include "fen.h"
#include "eval/eval.h"
#include "search/search.h"
#include "movegen/movegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Maximum absolute score to keep (positions beyond this are trivially won/lost) */
#define MAX_SCORE_CP 1000

/* Quiet threshold: max difference between qsearch and static eval */
#define QUIET_THRESHOLD 30

int run_tune_filter(const char *input_path, const char *output_path) {
    FILE *in = fopen(input_path, "r");
    if (!in) {
        perror("Failed to open input EPD file");
        return 1;
    }
    FILE *out = fopen(output_path, "w");
    if (!out) {
        perror("Failed to open output EPD file");
        fclose(in);
        return 1;
    }

    char line[1024];
    long long total_count = 0;
    long long quiet_count = 0;
    long long in_check_count = 0;
    long long extreme_count = 0;
    long long noisy_count = 0;

    printf("Filtering EPD file: %s -> %s\n", input_path, output_path);
    printf("  Quiet threshold: %d cp\n", QUIET_THRESHOLD);
    printf("  Max score:       %d cp\n", MAX_SCORE_CP);

    while (fgets(line, sizeof(line), in)) {
        // Strip newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) continue;

        // Parse FEN (first 6 tokens)
        char line_copy[1024];
        strcpy(line_copy, line);

        char *token = strtok(line_copy, " ");
        char fen[512] = "";
        int tokens_count = 0;
        
        while (token != NULL && tokens_count < 6) {
            if (tokens_count > 0) {
                strcat(fen, " ");
            }
            strcat(fen, token);
            tokens_count++;
            token = strtok(NULL, " ");
        }

        if (tokens_count < 6) {
            continue; // Invalid line
        }

        // Search for "score" in the original line
        char *score_ptr = strstr(line, "score");
        if (!score_ptr) {
            continue; // No score
        }

        int score_val = 0;
        if (sscanf(score_ptr, "score %d", &score_val) != 1) {
            continue; // Parse error
        }

        // Parse the position
        Position pos;
        memset(&pos, 0, sizeof(Position));
        if (fen_parse(fen, &pos) != 0) {
            continue; // FEN parse error
        }

        total_count++;

        // Filter 1: Reject extreme scores (trivially won/lost)
        if (abs(score_val) >= MAX_SCORE_CP) {
            extreme_count++;
            continue;
        }

        // Filter 2: Reject positions where side to move is in check
        Color stm = pos.sideToMove;
        int king_sq = pos.kingSq[COLOR_IDX(stm)];
        if (is_square_attacked(&pos, king_sq, OPPOSITE(stm))) {
            in_check_count++;
            continue;
        }

        // Filter 3: Run Q-search and compare to static eval for quietness
        int qscore = search_run_quiescence_only(&pos);

        // Get static evaluation from the side to move's perspective
        int static_eval = evaluate(&pos);
        if (pos.sideToMove == BLACK) {
            static_eval = -static_eval;
        }

        // If qsearch score differs from static evaluation by more than threshold,
        // it's a noisy/tactical position.
        int diff = abs(qscore - static_eval);
        if (diff > QUIET_THRESHOLD) {
            noisy_count++;
            continue;
        }

        quiet_count++;

        // Convert score from the side to move's perspective to White's perspective
        int score_white = (pos.sideToMove == WHITE) ? score_val : -score_val;

        // Map score to a continuous game outcome using sigmoid (Texel method).
        // K ≈ 1.13 is the standard scaling constant for centipawn scores.
        double simulated_result = 1.0 / (1.0 + pow(10.0, -1.13 * score_white / 400.0));

        // Write FEN, simulated result, raw white score, and qscore to the output file.
        // The qscore enables downstream stability analysis in classify_positions.py.
        fprintf(out, "%s | %.6f | %d | qscore %d\n", fen, simulated_result, score_white, qscore);

        if (quiet_count % 100000 == 0 && quiet_count > 0) {
            printf("  Progress: %lld processed, %lld quiet...\n", total_count, quiet_count);
        }
    }

    fclose(in);
    fclose(out);

    printf("Done. Filtering summary:\n");
    printf("  Total processed:   %lld\n", total_count);
    printf("  Quiet (exported):  %lld (%.2f%%)\n", quiet_count, (double)quiet_count / total_count * 100.0);
    printf("  Rejected in-check: %lld (%.2f%%)\n", in_check_count, (double)in_check_count / total_count * 100.0);
    printf("  Rejected extreme:  %lld (%.2f%%)\n", extreme_count, (double)extreme_count / total_count * 100.0);
    printf("  Rejected noisy:    %lld (%.2f%%)\n", noisy_count, (double)noisy_count / total_count * 100.0);

    return 0;
}
