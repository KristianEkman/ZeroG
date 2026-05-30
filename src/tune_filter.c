#include "tune_filter.h"
#include "boards.h"
#include "fen.h"
#include "eval/eval.h"
#include "search/search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

    printf("Filtering EPD file: %s -> %s\n", input_path, output_path);

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

        // Run Q-search to get the quiet score
        int qscore = search_run_quiescence_only(&pos);

        // Get static evaluation from the side to move's perspective
        int static_eval = evaluate(&pos);
        if (pos.sideToMove == BLACK) {
            static_eval = -static_eval;
        }

        // If qsearch score differs from static evaluation by <= 10 cp,
        // it's a quiet position.
        int diff = abs(qscore - static_eval);
        if (diff <= 10) {
            quiet_count++;

            // Convert score from the side to move's perspective to White's perspective
            int score_white = (pos.sideToMove == WHITE) ? score_val : -score_val;

            // Map score to a simulated game outcome (1.0 for win, 0.5 for draw, 0.0 for loss)
            // relative to White.
            double simulated_result = 0.5;
            if (score_white > 150) {
                simulated_result = 1.0;
            } else if (score_white < -150) {
                simulated_result = 0.0;
            }

            // Write FEN, simulated result, and raw white score to the output file
            fprintf(out, "%s | %.1f | %d\n", fen, simulated_result, score_white);
        }
    }

    fclose(in);
    fclose(out);

    printf("Done. Total processed positions: %lld, Quiet positions exported: %lld (%.2f%%)\n",
           total_count, quiet_count, (double)quiet_count / total_count * 100.0);

    return 0;
}
