#include "uci/uci_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "search/search.h"
#include "fen.h"
#include "movegen.h"

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

int uci_write_bestmove(const Position *board, FILE *output, Move move)
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
        if (uci_move_to_string(board, move, move_text, sizeof(move_text)) != 0)
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

static int is_move_tactical(const Position *board, Move m)
{
    if (m == 0)
    {
        return 0;
    }

    int from = MOVE_FROM(m);
    int to = MOVE_TO(m);
    Piece moving_piece = board->board[from];
    PieceType pt = PIECE_TYPE(moving_piece);

    // Capture
    if (board->board[to] != EMPTY)
    {
        return 1;
    }
    // En passant capture
    if (pt == PAWN && to == board->enPassantSquare)
    {
        return 1;
    }
    // Promotion
    if (pt == PAWN && (RANK_OF(to) == RANK_1 || RANK_OF(to) == RANK_8))
    {
        return 1;
    }

    return 0;
}

int uci_search_with_limits(const Position *board,
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

    if (search_status == 0 && result->has_legal_move)
    {
        const char *filename = uci_get_save_quiet_positions_file();
        if (filename && filename[0] != '\0')
        {
            int in_check = is_square_attacked(board, board->kingSq[COLOR_IDX(board->sideToMove)], OPPOSITE(board->sideToMove));
            if (!in_check && !is_move_tactical(board, result->best_move) && abs(result->score) < 1000)
            {
                char fen_buf[128];
                if (fen_serialize(board, fen_buf) > 0)
                {
                    FILE *f = fopen(filename, "a");
                    if (f != NULL)
                    {
                        fprintf(f, "%s score %d; depth %u;\n", fen_buf, result->score, result->depth);
                        fclose(f);
                    }
                }
            }
        }
    }

    return search_status;
}

int uci_search(const Position *board,
               unsigned depth,
               unsigned time_limit_ms,
               FILE *output,
               SearchResult *result)
{
    SearchLimits limits = {
        .depth = depth,
        .soft_time_limit_ms = time_limit_ms,
        .hard_time_limit_ms = time_limit_ms,
        .remaining_time_ms = 0,
        .increment_ms = 0,
        .movestogo = 0,
        .is_time_controlled = 0
    };

    return uci_search_with_limits(board, &limits, output, result);
}