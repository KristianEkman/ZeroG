#include "boards.h"
#include "uci.h"
#include "tune_filter.h"
#include "eval/tune_export.h"
#include "eval.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    srand((unsigned int)time(NULL));
    bitboard_init();

    if (argc > 1 && strcmp(argv[1], "--tune-filter") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s --tune-filter <input_file> <output_file>\n", argv[0]);
            return 1;
        }
        return run_tune_filter(argv[2], argv[3]);
    }

    if (argc > 1 && strcmp(argv[1], "--tune-export") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s --tune-export <input_file> <output_file>\n", argv[0]);
            return 1;
        }
        return run_tune_export(argv[2], argv[3]);
    }

    uci_run(stdin, stdout);
    eval_free();
    return 0;
}