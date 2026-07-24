#include "boards.h"
#include "uci.h"
#include "tune_filter.h"
#include "eval/tune_export.h"
#include "eval.h"
#include "nn/online_trainer.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    srand((unsigned int)time(NULL));
    bitboard_init();
    eval_init();

    if (argc > 1 && strcmp(argv[1], "--selfplay-train") == 0) {
        int games = (argc > 2) ? atoi(argv[2]) : 10;
        int depth = (argc > 3) ? atoi(argv[3]) : 7;
        int threads = (argc > 4) ? atoi(argv[4]) : 1;
        bool reset_network = (argc > 5) ? (atoi(argv[5]) != 0) : true;
        const char *weights_path = (argc > 6) ? argv[6] : "nn_weights.bin";

        int status = online_trainer_run_selfplay(&eval_nn, games, depth, threads, 0.0005f, 0.25f, reset_network, weights_path);
        eval_free();
        return status;
    }

    if (argc > 1 && strcmp(argv[1], "--tune-filter") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s --tune-filter <input_file> <output_file>\n", argv[0]);
            eval_free();
            return 1;
        }
        int status = run_tune_filter(argv[2], argv[3]);
        eval_free();
        return status;
    }

    if (argc > 1 && strcmp(argv[1], "--tune-export") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s --tune-export <input_file> <output_file>\n", argv[0]);
            eval_free();
            return 1;
        }
        int status = run_tune_export(argv[2], argv[3]);
        eval_free();
        return status;
    }

    uci_run(stdin, stdout);
    eval_free();
    return 0;
}