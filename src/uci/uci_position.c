#include "uci/uci_internal.h"

#include <ctype.h>
#include <string.h>

#include "fen.h"
#include "move_gen.h"

static const char *const uci_startpos_fen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static int parse_square_text(const char *text, Square *square)
{
    int file;
    int rank;

    if (text == NULL || square == NULL)
    {
        return -1;
    }

    if (text[0] < 'a' || text[0] > 'h' || text[1] < '1' || text[1] > '8')
    {
        return -1;
    }

    file = text[0] - 'a';
    rank = text[1] - '1';
    *square = (Square)(rank * 8 + file);
    return 0;
}

static int promotion_piece_type_from_char(char c, PieceType *piece_type)
{
    if (piece_type == NULL)
    {
        return -1;
    }

    switch (c)
    {
    case 'q':
        *piece_type = QUEEN;
        return 0;
    case 'r':
        *piece_type = ROOK;
        return 0;
    case 'b':
        *piece_type = BISHOP;
        return 0;
    case 'n':
        *piece_type = KNIGHT;
        return 0;
    default:
        return -1;
    }
}

static char promotion_char_from_piece_type(PieceType piece_type)
{
    switch (piece_type)
    {
    case QUEEN:
        return 'q';
    case ROOK:
        return 'r';
    case BISHOP:
        return 'b';
    case KNIGHT:
        return 'n';
    case NO_PIECE_TYPE:
    case PAWN:
    case KING:
    case PIECE_TYPE_COUNT:
        return '\0';
    }

    return '\0';
}

static int copy_fen_field(char *buffer,
                          size_t buffer_size,
                          size_t *cursor,
                          const char *field_start,
                          size_t field_length,
                          int add_separator)
{
    if (*cursor + field_length + (size_t)add_separator + 1 > buffer_size)
    {
        return -1;
    }

    memcpy(buffer + *cursor, field_start, field_length);
    *cursor += field_length;

    if (add_separator)
    {
        buffer[*cursor] = ' ';
        *cursor += 1;
    }

    buffer[*cursor] = '\0';
    return 0;
}

int uci_set_start_position(Board *board)
{
    return board_from_fen(board, uci_startpos_fen);
}

int uci_state_init(UciState *state)
{
    if (state == NULL)
    {
        return -1;
    }

    if (uci_set_start_position(&state->board) != 0)
    {
        return -1;
    }

    state->depth = UCI_DEFAULT_DEPTH;
    return 0;
}

int uci_move_to_string(Move move, char *buffer, size_t buffer_size)
{
    size_t required_length = move_is_promotion(move) ? 6 : 5;

    if (buffer == NULL || buffer_size < required_length)
    {
        return -1;
    }

    memcpy(buffer, square_names[move_from(move)], 2);
    memcpy(buffer + 2, square_names[move_to(move)], 2);

    if (move_is_promotion(move))
    {
        char promotion_char = promotion_char_from_piece_type(move_promotion_piece_type(move));

        if (promotion_char == '\0')
        {
            return -1;
        }

        buffer[4] = promotion_char;
        buffer[5] = '\0';
    }
    else
    {
        buffer[4] = '\0';
    }

    return 0;
}

int uci_parse_move(const Board *board, const char *text, Move *move)
{
    MoveList legal_moves;
    Square from;
    Square to;
    PieceType promotion_piece_type = NO_PIECE_TYPE;
    size_t text_length;

    if (board == NULL || text == NULL || move == NULL)
    {
        return -1;
    }

    text_length = strlen(text);
    if (text_length != 4 && text_length != 5)
    {
        return -1;
    }

    if (parse_square_text(text, &from) != 0 || parse_square_text(text + 2, &to) != 0)
    {
        return -1;
    }

    if (text_length == 5 && promotion_piece_type_from_char(text[4], &promotion_piece_type) != 0)
    {
        return -1;
    }

    if (move_gen_generate_legal(board, &legal_moves) != 0)
    {
        return -1;
    }

    for (size_t index = 0; index < legal_moves.count; ++index)
    {
        Move candidate = legal_moves.moves[index];

        if (move_from(candidate) != from || move_to(candidate) != to)
        {
            continue;
        }

        if (move_promotion_piece_type(candidate) != promotion_piece_type)
        {
            continue;
        }

        *move = candidate;
        return 0;
    }

    return -1;
}

int uci_apply_position(Board *board, const char *args)
{
    const char *cursor;

    if (board == NULL || args == NULL)
    {
        return -1;
    }

    cursor = uci_skip_spaces(args);

    if (uci_starts_with_keyword(cursor, "startpos", &cursor))
    {
        if (uci_set_start_position(board) != 0)
        {
            return -1;
        }
    }
    else if (uci_starts_with_keyword(cursor, "fen", &cursor))
    {
        char fen[UCI_FEN_CAPACITY];
        size_t fen_length = 0;

        for (int field = 0; field < 6; ++field)
        {
            const char *field_start = cursor;
            size_t field_length = 0;

            while (cursor[field_length] != '\0' && !isspace((unsigned char)cursor[field_length]))
            {
                ++field_length;
            }

            if (field_length == 0)
            {
                return -1;
            }

            if (copy_fen_field(fen, sizeof(fen), &fen_length, field_start, field_length, field < 5) != 0)
            {
                return -1;
            }

            cursor = uci_skip_spaces(cursor + field_length);
        }

        if (board_from_fen(board, fen) != 0)
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }

    if (*cursor == '\0')
    {
        return 0;
    }

    if (!uci_starts_with_keyword(cursor, "moves", &cursor))
    {
        return -1;
    }

    while (*cursor != '\0')
    {
        const char *move_start = cursor;
        size_t move_length = 0;
        char move_text[6];
        Move move;

        while (cursor[move_length] != '\0' && !isspace((unsigned char)cursor[move_length]))
        {
            ++move_length;
        }

        if (move_length == 0 || move_length >= sizeof(move_text))
        {
            return -1;
        }

        memcpy(move_text, move_start, move_length);
        move_text[move_length] = '\0';

        if (uci_parse_move(board, move_text, &move) != 0)
        {
            return -1;
        }

        if (apply_move(board, move, NULL) != 0)
        {
            return -1;
        }

        cursor = uci_skip_spaces(cursor + move_length);
    }

    return 0;
}