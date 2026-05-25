#include "uci/uci_internal.h"

#include <ctype.h>
#include <string.h>

#include "fen.h"
#include "movegen/movegen.h"

static const char *const uci_startpos_fen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static const char *const square_names[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"
};

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

int uci_set_start_position(Position *board)
{
    return fen_parse(uci_startpos_fen, board);
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

int uci_move_to_string(const Position *board, Move move, char *buffer, size_t buffer_size)
{
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    Piece p = board->board[from];
    PieceType pt = PIECE_TYPE(p);
    int is_promo = (pt == PAWN) && (RANK_OF(to) == 0 || RANK_OF(to) == 7);
    size_t required_length = is_promo ? 6 : 5;

    if (buffer == NULL || buffer_size < required_length)
    {
        return -1;
    }

    memcpy(buffer, square_names[from], 2);
    memcpy(buffer + 2, square_names[to], 2);

    if (is_promo)
    {
        int promo = MOVE_PROMO(move);
        /* promo: 0=N, 1=B, 2=R, 3=Q */
        char promo_chars[] = "nbrq";
        buffer[4] = promo_chars[promo];
        buffer[5] = '\0';
    }
    else
    {
        buffer[4] = '\0';
    }

    return 0;
}

int uci_parse_move(const Position *board, const char *text, Move *move)
{
    Move legal_moves[MAX_MOVES];
    Square from;
    Square to;
    int promotion_type = -1; /* -1 if not promotion, 0=N, 1=B, 2=R, 3=Q */
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

    if (text_length == 5)
    {
        switch (text[4])
        {
        case 'n': promotion_type = 0; break;
        case 'b': promotion_type = 1; break;
        case 'r': promotion_type = 2; break;
        case 'q': promotion_type = 3; break;
        default: return -1;
        }
    }

    int count = movegen_legal(board, legal_moves);
    for (int index = 0; index < count; ++index)
    {
        Move candidate = legal_moves[index];

        if (MOVE_FROM(candidate) != from || MOVE_TO(candidate) != to)
        {
            continue;
        }

        /* If the candidate is a promotion, check if it matches the parsed promotion type */
        Piece p = board->board[from];
        PieceType pt = PIECE_TYPE(p);
        int is_candidate_promo = (pt == PAWN) && (RANK_OF(to) == 0 || RANK_OF(to) == 7);

        if (is_candidate_promo)
        {
            if (MOVE_PROMO(candidate) != promotion_type)
            {
                continue;
            }
        }
        else
        {
            if (promotion_type != -1)
            {
                continue;
            }
        }

        *move = candidate;
        return 0;
    }

    return -1;
}

int uci_apply_position(Position *board, const char *args)
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

        if (fen_parse(fen, board) != 0)
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

        Undo u;
        apply_move(board, move, &u);

        cursor = uci_skip_spaces(cursor + move_length);
    }

    return 0;
}