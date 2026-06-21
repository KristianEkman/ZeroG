#ifndef SEARCH_INTERNAL_H
#define SEARCH_INTERNAL_H

#include "search/search.h"
#include "eval/eval.h"
#include "movegen/movegen.h"
#include "search/time_control.h"
#include "transposition_table.h"
#include "uci/uci.h"
#include "zobrist.h"
#include "see.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#define MAX_DEPTH 64
#define INFINITY_SCORE 30000
#define MATE_SCORE 29000

typedef struct {
  int length;
  Move moves[MAX_DEPTH];
} PVLine;

typedef struct UndoNode {
  const Undo *undo;
  const struct UndoNode *parent;
} UndoNode;

/* Globals shared across search files */
extern FILE *search_log_output;
extern int stop_requested;
extern unsigned hash_size;
extern int lmr_base;
extern int futility_margin;
extern int rfp_margin_base;
extern int nmp_min_depth;
extern int singular_margin;
extern int aspiration_window;
extern int lmr_min_depth;
extern int futility_max_depth;
extern int lmr_history_divisor;
extern uint64_t node_count;

extern Move killer_moves[2][MAX_DEPTH];
extern int history_scores[2][64][64];
extern Move countermoves[2][64][64];

extern int lmr_reductions[MAX_DEPTH][MAX_MOVES];
extern int lmr_initialized;

extern TranspositionTable tt;
extern int tt_initialized;

/* Global Init Helper */
void init_lmr(void);

/* Search Utilities (search_utils.c) */
uint64_t get_time_ms(void);
int has_insufficient_material(const Position *pos);
int is_draw(const Position *pos);
int is_repetition(const Position *pos, const UndoNode *history, const SearchLimits *limits);
int is_illegal_castle(const Position *pos, Move m);
int make_move_and_check_legal(Position *pos, Move m, Undo *u);
int evaluate_no_moves(int in_check, int ply);
void check_time_limit(uint64_t start_time, const SearchLimits *limits);

/* Move Ordering (move_ordering.c) */
int move_is_capture_or_promo(const Position *pos, Move m);
int score_move(const Position *pos, Move m, Move pv_move, int ply, Move prev_move);
void pick_best_move(Move *moves, int *scores, int count, int current_idx);
void update_quiet_move_heuristics(const Position *pos, Move cut_move, Move *moves, int tried_count, int depth, int ply, Move prev_move);

/* PVS / Alpha-Beta (pvs.c) */
int pvs(Position *pos, int depth, int ply, int alpha, int beta,
        PVLine *pv, uint64_t start_time, const SearchLimits *limits,
        Move pv_move, const UndoNode *history, int allow_nmp,
        Move excluded_move, Move prev_move);
int quiescence(Position *pos, int ply, int alpha, int beta,
              uint64_t start_time, const SearchLimits *limits);
int try_null_move_pruning(Position *pos, int depth, int ply, int beta,
                          uint64_t start_time, const SearchLimits *limits,
                          const UndoNode *history);
int search_aspiration_window(Position *pos, unsigned depth,
                            int previous_score, PVLine *pv,
                            uint64_t start_time, const SearchLimits *limits,
                            Move best_move_so_far);

/* Search Main helpers (search.c / internal to it) */
int reconstruct_pv_from_tt(const Position *pos, PVLine *pv, int max_depth);
void log_search_info(unsigned depth, int score, uint64_t nodes,
                     const PVLine *pv, const Position *board);

#endif /* SEARCH_INTERNAL_H */
