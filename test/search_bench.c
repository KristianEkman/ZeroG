#include "boards.h"
#include "fen.h"
#include "search/search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static double now_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

typedef struct {
    const char *name;
    const char *fen;
} BenchPosition;

static const BenchPosition positions[] = {
    {"Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
    {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
    {"Position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"},
    {"Position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"},
    {NULL, NULL}
};

int main(int argc, char **argv)
{
    int target_depth = 7;
    if (argc > 1) {
        target_depth = atoi(argv[1]);
    }

    bitboard_init();

    printf("==================================================\n");
    printf("            SEARCH BENCHMARK (Depth = %d)         \n", target_depth);
    printf("==================================================\n");

    uint64_t total_nodes = 0;
    double total_time = 0.0;

    for (int i = 0; positions[i].name != NULL; i++) {
        Position pos;
        memset(&pos, 0, sizeof(Position));
        if (fen_parse(positions[i].fen, &pos) != 0) {
            fprintf(stderr, "Failed to parse FEN for %s\n", positions[i].name);
            continue;
        }

        SearchLimits limits = {
            .depth = target_depth,
            .soft_time_limit_ms = 0,
            .hard_time_limit_ms = 0
        };
        SearchResult result;
        memset(&result, 0, sizeof(SearchResult));

        double t0 = now_sec();
        search_best_move_with_limits(&pos, &limits, &result);
        double t1 = now_sec();

        double elapsed = t1 - t0;
        uint64_t nodes = result.node_count;
        double nps = (elapsed > 0.0) ? (double)nodes / elapsed : 0.0;

        printf("Position: %s\n", positions[i].name);
        printf("  Nodes  : %llu\n", (unsigned long long)nodes);
        printf("  Time   : %.4f s\n", elapsed);
        printf("  NPS    : %.0f\n", nps);
        printf("--------------------------------------------------\n");

        total_nodes += nodes;
        total_time += elapsed;
    }

    double overall_nps = (total_time > 0.0) ? (double)total_nodes / total_time : 0.0;
    printf("OVERALL SUMMARY:\n");
    printf("  Total Nodes: %llu\n", (unsigned long long)total_nodes);
    printf("  Total Time : %.4f s\n", total_time);
    printf("  Overall NPS: %.0f\n", overall_nps);
    printf("==================================================\n");

    return 0;
}
