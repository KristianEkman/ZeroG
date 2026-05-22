#include "boards.h"
#include "movegen.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

static double now_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

int main(int argc, char **argv)
{
    int depth = 5;
    if (argc > 1)
    {
        depth = atoi(argv[1]);
        if (depth < 0)
        {
            fprintf(stderr, "Depth must be >= 0\n");
            return 1;
        }
    }

    Position pos;
    bitboard_init();
    position_startpos(&pos);

    double t0 = now_sec();
    uint64_t nodes = perft(&pos, depth);
    double t1 = now_sec();

    double elapsed = t1 - t0;
    double nps = (elapsed > 0.0) ? (double)nodes / elapsed : 0.0;

    printf("perft startpos depth=%d\n", depth);
    printf("nodes=%llu\n", (unsigned long long)nodes);
    printf("time=%.6f s\n", elapsed);
    printf("moves/s=%.0f\n", nps);

    return 0;
}
