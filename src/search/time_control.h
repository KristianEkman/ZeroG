#ifndef TIME_CONTROL_H
#define TIME_CONTROL_H

#include "boards.h"

typedef struct {
    uint64_t start_time_ms;
    
    unsigned original_soft_limit_ms;
    unsigned original_hard_limit_ms;
    
    unsigned soft_limit_ms;
    unsigned hard_limit_ms;
    
    Move best_moves[64];
    int scores[64];
    uint64_t nodes[64];
    unsigned depth_count;
    
    int num_legal_moves;
} TimeControl;

// Computes and initializes time control parameters based on board and search constraints
void time_control_init(TimeControl *tc,
                       const Position *board,
                       unsigned remaining_ms,
                       unsigned increment_ms,
                       unsigned movestogo,
                       int num_legal_moves);

// Updates time control state at the end of a depth iteration.
// Returns 1 if search should be stopped immediately (breaking the iterative deepening loop),
// 0 otherwise.
int time_control_update(TimeControl *tc,
                        unsigned depth,
                        int score,
                        Move best_move,
                        uint64_t current_nodes,
                        uint64_t elapsed_ms);

#endif // TIME_CONTROL_H
