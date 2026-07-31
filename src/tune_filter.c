#include "tune_filter.h"
#include "boards.h"
#include "fen.h"
#include "eval/eval.h"
#include "search/search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

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

        // Parse line format:
        // Format A (Pipe-delimited): <FEN> | <simulated_result> | <score_white>
        // Format B (Opcode-based):   <FEN> c0 ...; score <score_val>; depth <depth>;
        
        char fen[512] = "";
        int score_val = 0;
        int has_score = 0;
        int score_is_white = 0;

        char *pipe1 = strchr(line, '|');
        if (pipe1) {
            // Extract FEN part (everything before the first '|')
            size_t fen_len = (size_t)(pipe1 - line);
            if (fen_len >= sizeof(fen)) fen_len = sizeof(fen) - 1;
            strncpy(fen, line, fen_len);
            fen[fen_len] = '\0';
            
            // Trim trailing spaces from FEN
            while (fen_len > 0 && isspace((unsigned char)fen[fen_len - 1])) {
                fen[--fen_len] = '\0';
            }

            char *pipe2 = strchr(pipe1 + 1, '|');
            if (pipe2) {
                if (sscanf(pipe2 + 1, " %d", &score_val) == 1) {
                    has_score = 1;
                    score_is_white = 1;
                }
            }
        } else {
            // Opcode format: extract first 6 space-separated tokens as FEN
            char line_copy[1024];
            strncpy(line_copy, line, sizeof(line_copy) - 1);
            line_copy[sizeof(line_copy) - 1] = '\0';

            char *token = strtok(line_copy, " ");
            int tokens_count = 0;
            while (token != NULL && tokens_count < 6) {
                if (tokens_count > 0) strcat(fen, " ");
                strcat(fen, token);
                tokens_count++;
                token = strtok(NULL, " ");
            }

            if (tokens_count >= 4) { // FEN requires at least 4 fields
                char *score_ptr = strstr(line, "score");
                if (score_ptr) {
                    if (sscanf(score_ptr, "score %d", &score_val) == 1) {
                        has_score = 1;
                        score_is_white = 0;
                    }
                }
            }
        }

        if (!has_score || fen[0] == '\0') {
            continue; // Invalid line or missing score
        }

        // Parse the position
        Position pos;
        memset(&pos, 0, sizeof(Position));
        if (fen_parse(fen, &pos) != 0) {
            continue; // FEN parse error
        }

        if (use_nn && eval_nn) {
            nnue_refresh_accumulator(eval_nn, &pos);
        }

        total_count++;

        // Run Q-search to get the quiet score (from side-to-move's perspective)
        int qscore = search_run_quiescence_only(&pos);

        // Get static evaluation from side-to-move's perspective
        int static_eval = evaluate(&pos);
        if (pos.sideToMove == BLACK) {
            static_eval = -static_eval;
        }

        // If qsearch score differs from static evaluation by <= 10 cp, it's a quiet position.
        int diff = abs(qscore - static_eval);
        if (diff <= 10) {
            quiet_count++;

            int score_white = score_is_white ? score_val : ((pos.sideToMove == WHITE) ? score_val : -score_val);

            // Map score to a continuous game outcome using sigmoid (Texel method / WDL)
            double simulated_result = 1.0 / (1.0 + exp(- (double)score_white / 400.0));

            // Write FEN, simulated result, and raw white score to output file
            fprintf(out, "%s | %.6f | %d\n", fen, simulated_result, score_white);
        }
    }

    fclose(in);
    fclose(out);

    printf("Done. Total processed positions: %lld, Quiet positions exported: %lld (%.2f%%)\n",
           total_count, quiet_count, (double)quiet_count / total_count * 100.0);

    return 0;
}
