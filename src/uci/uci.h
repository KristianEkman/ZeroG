#ifndef UCI_H
#define UCI_H

#include <stddef.h>
#include <stdio.h>

#include "boards.h"

typedef struct {
    Position board;
    unsigned depth;
    uint64_t history_hashes[1024];
    int history_count;
} UciState;

int uci_state_init(UciState *state);
int uci_move_to_string(const Position *board, Move move, char *buffer, size_t buffer_size);
int uci_parse_move(const Position *board, const char *text, Move *move);
int uci_apply_position(UciState *state, const char *args);
int uci_handle_line(UciState *state, const char *line, FILE *output, int *should_quit);
void uci_run(FILE *input, FILE *output);

#endif