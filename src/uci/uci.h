#ifndef UCI_H
#define UCI_H

#include <stddef.h>
#include <stdio.h>

#include "board.h"
#include "move.h"

typedef struct {
    Board board;
    unsigned depth;
} UciState;

int uci_state_init(UciState *state);
int uci_move_to_string(Move move, char *buffer, size_t buffer_size);
int uci_parse_move(const Board *board, const char *text, Move *move);
int uci_apply_position(Board *board, const char *args);
int uci_handle_line(UciState *state, const char *line, FILE *output, int *should_quit);
void uci_run(FILE *input, FILE *output);

#endif