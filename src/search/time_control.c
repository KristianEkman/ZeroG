#include "time_control.h"
#include <stdlib.h>

void time_control_init(TimeControl *tc,
                       const Position *board,
                       unsigned remaining_ms,
                       unsigned increment_ms,
                       unsigned movestogo,
                       int num_legal_moves) {
    tc->depth_count = 0;
    tc->num_legal_moves = num_legal_moves;
    
    // Initialize history arrays
    for (int i = 0; i < 64; i++) {
        tc->best_moves[i] = 0;
        tc->scores[i] = 0;
        tc->nodes[i] = 0;
    }

    // Safety overhead
    unsigned overhead = 50; // 50ms safety margin
    unsigned usable_time = remaining_ms > overhead ? remaining_ms - overhead : remaining_ms / 2;

    // Estimate moves to go if not provided
    unsigned moves_to_go = movestogo;
    if (moves_to_go == 0) {
        int phase = get_game_phase(board);
        double phase_factor = (double)phase / 24.0; // 0.0 to 1.0
        
        // We assume 20 moves left in endgame (phase=0), up to 40 moves in opening/middlegame (phase=24)
        moves_to_go = 20 + (unsigned)(20.0 * phase_factor);
        
        // Adjust for fullmoveNumber to avoid too small or too large values
        int move_num = board->fullmoveNumber;
        if (move_num > 40) {
            // As the game goes on and we are in endgame, budget tighter
            unsigned scale_down = (move_num - 40) / 4;
            if (moves_to_go > scale_down + 15) {
                moves_to_go -= scale_down;
            } else {
                moves_to_go = 15;
            }
        }
        if (moves_to_go < 15) {
            moves_to_go = 15;
        }
    }

    // Calculate soft limit: spending usable_time / moves_to_go + 80% of increment
    unsigned soft = usable_time / moves_to_go + (increment_ms * 4) / 5;
    
    // Calculate hard limit: maximum allowed time.
    // Typically usable_time / 4 + increment, but capped to avoid flagging.
    unsigned hard = usable_time / 4 + increment_ms;
    
    // Capping hard limit to at most 80% of usable time to guarantee no timeout
    if (hard > usable_time * 8 / 10) {
        hard = usable_time * 8 / 10;
    }
    
    // Ensure soft limit does not exceed hard limit
    if (soft > hard) {
        soft = hard;
    }
    
    // Extreme safety limits for tiny remaining times
    if (hard < 20 && remaining_ms > 20) {
        hard = remaining_ms - 20;
    }
    if (soft >= hard) {
        soft = (hard * 8) / 10;
    }
    if (soft < 5) {
        soft = 5;
    }
    if (hard < 10) {
        hard = 10;
    }

    tc->original_soft_limit_ms = soft;
    tc->original_hard_limit_ms = hard;
    tc->soft_limit_ms = soft;
    tc->hard_limit_ms = hard;
}

int time_control_update(TimeControl *tc,
                        unsigned depth,
                        int score,
                        Move best_move,
                        uint64_t current_nodes,
                        uint64_t elapsed_ms) {
    if (depth >= 64) {
        depth = 63;
    }
    tc->best_moves[depth] = best_move;
    tc->scores[depth] = score;
    tc->nodes[depth] = current_nodes;
    tc->depth_count = depth;

    // 1. Only 1 legal move at root
    if (tc->num_legal_moves == 1) {
        // Only one option, no point in searching deeper than depth 1
        return 1;
    }

    // 2. Adjust soft limit based on move stability at root
    // Reset to original soft limit first
    tc->soft_limit_ms = tc->original_soft_limit_ms;

    if (depth >= 4) {
        Move m = tc->best_moves[depth];
        if (m == tc->best_moves[depth - 1] && m == tc->best_moves[depth - 2]) {
            if (depth >= 6 && m == tc->best_moves[depth - 3]) {
                // Highly stable: scale down soft limit by 50%
                tc->soft_limit_ms = (tc->original_soft_limit_ms * 5) / 10;
            } else {
                // Stable: scale down soft limit by 75%
                tc->soft_limit_ms = (tc->original_soft_limit_ms * 75) / 100;
            }
        }
        
        // If the best move just changed, scale up soft limit by 150% (instability)
        if (m != tc->best_moves[depth - 1]) {
            tc->soft_limit_ms = (tc->original_soft_limit_ms * 15) / 10;
            // Cap at 80% of hard limit
            if (tc->soft_limit_ms > (tc->hard_limit_ms * 8) / 10) {
                tc->soft_limit_ms = (tc->hard_limit_ms * 8) / 10;
            }
        }
    }

    // 3. Score-drop extension: invest more time when score is falling
    if (depth >= 6) {
        int baseline = (tc->scores[depth - 1] + tc->scores[depth - 2]) / 2;
        int score_diff = score - baseline;  // negative = score dropped

        double score_factor = 1.0;
        if (score_diff < -50) {
            // Large drop: extend soft limit significantly (up to 2x)
            score_factor = 1.5 + (double)(-score_diff - 50) / 200.0;
            if (score_factor > 2.0) score_factor = 2.0;
        } else if (score_diff < -20) {
            // Moderate drop: slight extension
            score_factor = 1.0 + (double)(-score_diff - 20) / 100.0;
        } else if (score_diff > 30) {
            // Score improving: save time
            score_factor = 0.85;
        }

        tc->soft_limit_ms = (unsigned)(tc->soft_limit_ms * score_factor);

        // Re-cap at hard limit
        if (tc->soft_limit_ms > (tc->hard_limit_ms * 9) / 10) {
            tc->soft_limit_ms = (tc->hard_limit_ms * 9) / 10;
        }
    }

    // 4. Predict if we will complete the next depth iteration
    double factor = 3.0; // default node branching multiplier
    if (depth >= 2 && tc->nodes[depth - 1] > 0) {
        factor = (double)current_nodes / (double)tc->nodes[depth - 1];
    }
    if (factor < 1.5) factor = 1.5;
    if (factor > 5.0) factor = 5.0;

    uint64_t estimated_total_next_depth_time = (uint64_t)(elapsed_ms * factor);
    if (estimated_total_next_depth_time > tc->soft_limit_ms) {
        // The next depth will highly likely overshoot the soft limit, so stop now and save time
        return 1;
    }

    return 0;
}
