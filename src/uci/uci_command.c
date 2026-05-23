#include "uci/uci_internal.h"

#include <stdio.h>

static int write_uci_handshake(FILE *output)
{
    if (fprintf(output, "id name ChessAI2026 1.0.0\n") < 0 ||
        fprintf(output, "id author Kristian Ekman\n") < 0 ||
        fprintf(output, "option name Hash type spin default 16 min 1 max 1024\n") < 0 ||
        fprintf(output, "uciok\n") < 0)
    {
        return -1;
    }

    fflush(output);
    return 0;
}

int uci_handle_line(UciState *state, const char *line, FILE *output, int *should_quit)
{
    const char *args;

    if (state == NULL || line == NULL || output == NULL || should_quit == NULL)
    {
        return -1;
    }

    *should_quit = 0;
    line = uci_skip_spaces(line);

    if (*line == '\0')
    {
        return 0;
    }

    if (uci_starts_with_keyword(line, "uci", &args))
    {
        (void)args;
        return write_uci_handshake(output);
    }

    if (uci_starts_with_keyword(line, "isready", &args))
    {
        (void)args;
        if (fprintf(output, "readyok\n") < 0)
        {
            return -1;
        }

        fflush(output);
        return 0;
    }

    if (uci_starts_with_keyword(line, "ucinewgame", &args))
    {
        (void)args;
        return uci_set_start_position(&state->board);
    }

    if (uci_starts_with_keyword(line, "position", &args))
    {
        return uci_apply_position(&state->board, args);
    }

    if (uci_starts_with_keyword(line, "go", &args))
    {
        SearchResult result;
        SearchLimits limits;

        if (uci_parse_go_search_limits(state, args, &limits) != 0)
        {
            return -1;
        }

        if (uci_search_with_limits(&state->board, &limits, output, &result) != 0)
        {
            return -1;
        }

        return uci_write_bestmove(output, result.has_legal_move ? result.best_move : 0);
    }

    if (uci_starts_with_keyword(line, "setoption", &args))
    {
        unsigned hash_size_mb;

        if (uci_parse_hash_option_value(args, &hash_size_mb) != 0)
        {
            return -1;
        }

        return search_set_hash_size_mb(hash_size_mb);
    }

    if (uci_starts_with_keyword(line, "stop", &args) ||
        uci_starts_with_keyword(line, "ponderhit", &args))
    {
        (void)args;
        return 0;
    }

    if (uci_starts_with_keyword(line, "quit", &args))
    {
        (void)args;
        *should_quit = 1;
        return 0;
    }

    return -1;
}