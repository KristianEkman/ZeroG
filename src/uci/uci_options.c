#include "uci/uci_internal.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>

typedef struct
{
    int has_depth;
    unsigned depth;
    int has_movetime;
    unsigned movetime_ms;
    int has_wtime;
    unsigned wtime_ms;
    int has_btime;
    unsigned btime_ms;
    int has_winc;
    unsigned winc_ms;
    int has_binc;
    unsigned binc_ms;
} UciGoOptions;

static int parse_unsigned_token(const char *text, unsigned *value, const char **remainder);

static int uci_parse_go_options(const char *args,
                                unsigned default_depth,
                                UciGoOptions *options)
{
    const char *cursor = uci_skip_spaces(args);

    if (options == NULL)
    {
        return -1;
    }

    memset(options, 0, sizeof(*options));
    options->depth = default_depth;

    while (*cursor != '\0')
    {
        const char *token_start = cursor;
        size_t token_length = 0;
        unsigned parsed_value = 0;
        unsigned *target_value = NULL;
        int *target_flag = NULL;

        while (cursor[token_length] != '\0' && !isspace((unsigned char)cursor[token_length]))
        {
            ++token_length;
        }

        if (token_length == 5 && strncmp(token_start, "depth", 5) == 0)
        {
            target_value = &options->depth;
            target_flag = &options->has_depth;
        }
        else if (token_length == 8 && strncmp(token_start, "movetime", 8) == 0)
        {
            target_value = &options->movetime_ms;
            target_flag = &options->has_movetime;
        }
        else if (token_length == 5 && strncmp(token_start, "wtime", 5) == 0)
        {
            target_value = &options->wtime_ms;
            target_flag = &options->has_wtime;
        }
        else if (token_length == 5 && strncmp(token_start, "btime", 5) == 0)
        {
            target_value = &options->btime_ms;
            target_flag = &options->has_btime;
        }
        else if (token_length == 4 && strncmp(token_start, "winc", 4) == 0)
        {
            target_value = &options->winc_ms;
            target_flag = &options->has_winc;
        }
        else if (token_length == 4 && strncmp(token_start, "binc", 4) == 0)
        {
            target_value = &options->binc_ms;
            target_flag = &options->has_binc;
        }

        if (target_value != NULL)
        {
            const char *remainder = NULL;

            cursor = uci_skip_spaces(cursor + token_length);
            if (parse_unsigned_token(cursor, &parsed_value, &remainder) != 0)
            {
                return -1;
            }

            if (*remainder != '\0' && !isspace((unsigned char)*remainder))
            {
                return -1;
            }

            *target_value = parsed_value;
            *target_flag = 1;
            cursor = remainder;
            continue;
        }

        cursor = uci_skip_spaces(cursor + token_length);
    }

    return 0;
}

static int parse_unsigned_token(const char *text, unsigned *value, const char **remainder)
{
    unsigned parsed_value = 0;
    const char *cursor = text;

    if (text == NULL || value == NULL)
    {
        return -1;
    }

    if (!isdigit((unsigned char)*cursor))
    {
        return -1;
    }

    while (isdigit((unsigned char)*cursor))
    {
        unsigned digit = (unsigned)(*cursor - '0');

        if (parsed_value > ((UINT_MAX - digit) / 10u))
        {
            return -1;
        }

        parsed_value = (parsed_value * 10u) + digit;
        ++cursor;
    }

    *value = parsed_value;
    if (remainder != NULL)
    {
        *remainder = cursor;
    }
    return 0;
}

int uci_parse_hash_option_value(const char *args, unsigned *value)
{
    const char *cursor = uci_skip_spaces(args);

    if (value == NULL)
    {
        return -1;
    }

    if (!uci_starts_with_keyword(cursor, "name", &cursor))
    {
        return -1;
    }

    if (!uci_starts_with_keyword(cursor, "Hash", &cursor))
    {
        return -1;
    }

    if (!uci_starts_with_keyword(cursor, "value", &cursor))
    {
        return -1;
    }

    if (parse_unsigned_token(cursor, value, &cursor) != 0)
    {
        return -1;
    }

    cursor = uci_skip_spaces(cursor);
    return *cursor == '\0' ? 0 : -1;
}

int uci_parse_go_search_limits(const UciState *state,
                               const char *args,
                               SearchLimits *limits)
{
    UciGoOptions options;
    unsigned remaining_ms = 0;
    unsigned increment_ms = 0;
    int has_time_control = 0;
    int has_time_limit = 0;

    if (state == NULL || limits == NULL)
    {
        return -1;
    }

    if (uci_parse_go_options(args, state->depth, &options) != 0)
    {
        return -1;
    }

    limits->depth = options.depth;
    limits->soft_time_limit_ms = 0u;
    limits->hard_time_limit_ms = 0u;

    if (options.has_movetime)
    {
        limits->soft_time_limit_ms = options.movetime_ms;
        limits->hard_time_limit_ms = options.movetime_ms;
        has_time_limit = options.movetime_ms > 0u;
    }
    else if (state->board.sideToMove == WHITE)
    {
        remaining_ms = options.wtime_ms;
        increment_ms = options.winc_ms;
        has_time_control = options.has_wtime;
    }
    else
    {
        remaining_ms = options.btime_ms;
        increment_ms = options.binc_ms;
        has_time_control = options.has_btime;
    }

    if (!options.has_movetime && has_time_control)
    {
        if (search_compute_time_limits(&state->board,
                                       options.has_depth ? options.depth : UINT_MAX,
                                       remaining_ms,
                                       increment_ms,
                                       limits) != 0)
        {
            return -1;
        }

        has_time_limit = limits->hard_time_limit_ms > 0u;
    }

    if (has_time_limit && !options.has_depth)
    {
        limits->depth = UINT_MAX;
    }

    return 0;
}

int uci_parse_go_parameters(const UciState *state,
                            const char *args,
                            unsigned *depth,
                            unsigned *time_limit_ms)
{
    SearchLimits limits;

    if (depth == NULL || time_limit_ms == NULL)
    {
        return -1;
    }

    if (uci_parse_go_search_limits(state, args, &limits) != 0)
    {
        return -1;
    }

    *depth = limits.depth;
    *time_limit_ms = limits.hard_time_limit_ms;
    return 0;
}