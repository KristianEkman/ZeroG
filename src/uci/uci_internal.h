#ifndef UCI_INTERNAL_H
#define UCI_INTERNAL_H

#include <stdio.h>

#include "search/search.h"
#include "uci.h"

enum
{
    UCI_LINE_CAPACITY = 1024,
    UCI_FEN_CAPACITY = 128,
    UCI_DEFAULT_DEPTH = 4
};

const char *uci_skip_spaces(const char *text);
int uci_starts_with_keyword(const char *line, const char *keyword, const char **remainder);
int uci_line_starts_with(const char *line, const char *keyword, const char **args);
int uci_set_start_position(Board *board);
int uci_write_bestmove(FILE *output, Move move);
int uci_search_with_limits(const Board *board,
                           const SearchLimits *limits,
                           FILE *output,
                           SearchResult *result);
int uci_search(const Board *board,
               unsigned depth,
               unsigned time_limit_ms,
               FILE *output,
               SearchResult *result);
int uci_parse_go_search_limits(const UciState *state,
                               const char *args,
                               SearchLimits *limits);
int uci_parse_go_parameters(const UciState *state,
                            const char *args,
                            unsigned *depth,
                            unsigned *time_limit_ms);
int uci_parse_hash_option_value(const char *args, unsigned *value);

#endif