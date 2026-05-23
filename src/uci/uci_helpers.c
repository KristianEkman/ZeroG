#include "uci/uci_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "search/search.h"

const char *uci_skip_spaces(const char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text))
    {
        ++text;
    }

    return text;
}

int uci_starts_with_keyword(const char *line, const char *keyword, const char **remainder)
{
    size_t keyword_length;

    if (line == NULL || keyword == NULL)
    {
        return 0;
    }

    keyword_length = strlen(keyword);
    if (strncmp(line, keyword, keyword_length) != 0)
    {
        return 0;
    }

    if (line[keyword_length] != '\0' && !isspace((unsigned char)line[keyword_length]))
    {
        return 0;
    }

    if (remainder != NULL)
    {
        *remainder = uci_skip_spaces(line + keyword_length);
    }

    return 1;
}

int uci_line_starts_with(const char *line, const char *keyword, const char **args)
{
    return uci_starts_with_keyword(uci_skip_spaces(line), keyword, args);
}

int uci_write_bestmove(FILE *output, Move move)
{
    char move_text[6];

    if (output == NULL)
    {
        return -1;
    }

    if (move == 0)
    {
        if (fprintf(output, "bestmove 0000\n") < 0)
        {
            return -1;
        }
    }
    else
    {
        if (uci_move_to_string(move, move_text, sizeof(move_text)) != 0)
        {
            return -1;
        }

        if (fprintf(output, "bestmove %s\n", move_text) < 0)
        {
            return -1;
        }
    }

    fflush(output);
    return 0;
}

int uci_search_with_limits(const Board *board,
                           const SearchLimits *limits,
                           FILE *output,
                           SearchResult *result)
{
    FILE *previous_log_output;
    int search_status;

    if (board == NULL || limits == NULL || output == NULL || result == NULL)
    {
        return -1;
    }

    previous_log_output = search_set_log_output(output);
    search_status = search_best_move_with_limits(board, limits, result);
    (void)search_set_log_output(previous_log_output);
    return search_status;
}

int uci_search(const Board *board,
               unsigned depth,
               unsigned time_limit_ms,
               FILE *output,
               SearchResult *result)
{
    SearchLimits limits = {
        depth,
        time_limit_ms,
        time_limit_ms,
    };

    return uci_search_with_limits(board, &limits, output, result);
}