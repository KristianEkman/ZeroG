#include "fen.h"

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "move/move_gen.h"
#include "zobrist.h"

static Piece piece_from_fen_char(char c)
{
    Color color;
    PieceType piece_type;

    switch (c)
    {
    case 'P':
    case 'N':
    case 'B':
    case 'R':
    case 'Q':
    case 'K':
        color = WHITE;
        break;
    case 'p':
    case 'n':
    case 'b':
    case 'r':
    case 'q':
    case 'k':
        color = BLACK;
        break;
    default:
        return NO_PIECE;
    }

    switch (tolower((unsigned char)c))
    {
    case 'p':
        piece_type = PAWN;
        break;
    case 'n':
        piece_type = KNIGHT;
        break;
    case 'b':
        piece_type = BISHOP;
        break;
    case 'r':
        piece_type = ROOK;
        break;
    case 'q':
        piece_type = QUEEN;
        break;
    case 'k':
        piece_type = KING;
        break;
    default:
        return NO_PIECE;
    }

    return make_piece(color, piece_type);
}

static int parse_fen_board(Board *board, const char **cursor)
{
    const char *p = *cursor;

    for (int rank = 7; rank >= 0; --rank)
    {
        int file = 0;

        while (file < 8)
        {
            Piece piece;
            Square square;

            if (*p >= '1' && *p <= '8')
            {
                file += *p - '0';
                ++p;
                continue;
            }

            piece = piece_from_fen_char(*p);
            if (piece == NO_PIECE)
            {
                return -1;
            }

            square = (Square)((rank * 8) + file);
            board->squares[square] = piece;
            board->material_score += piece_material_value(piece);
            board->game_phase += piece_game_phase_value(piece);
            board->material_state = board_material_state_add_piece(board->material_state,
                                                                   piece,
                                                                   square);

            {
                Color color = piece_get_color(piece);
                PieceType piece_type = piece_get_type(piece);
                uint64_t square_mask = UINT64_C(1) << square;

                board->piece_bitboards[color][piece_type] |= square_mask;
                board->occupancy_bitboards[color] |= square_mask;
                board->occupancy_all |= square_mask;
            }

            if (piece_get_type(piece) == KING)
            {
                board->king_squares[piece_get_color(piece)] = square;
            }

            ++file;
            ++p;
        }

        if (rank > 0)
        {
            if (*p != '/')
            {
                return -1;
            }
            ++p;
        }
    }

    if (*p != ' ')
    {
        return -1;
    }

    *cursor = p + 1;
    return 0;
}

static int parse_fen_side_to_move(Board *board, const char **cursor)
{
    const char *p = *cursor;

    if (p[0] == 'w' && p[1] == ' ')
    {
        board->side_to_move = WHITE;
        *cursor = p + 2;
        return 0;
    }

    if (p[0] == 'b' && p[1] == ' ')
    {
        board->side_to_move = BLACK;
        *cursor = p + 2;
        return 0;
    }

    return -1;
}

static int parse_fen_castling(Board *board, const char **cursor)
{
    const char *p = *cursor;
    unsigned rights = 0;

    if (*p == '-')
    {
        ++p;
    }
    else
    {
        while (*p != ' ')
        {
            unsigned right;

            switch (*p)
            {
            case 'K':
                right = CASTLE_WHITE_KINGSIDE;
                break;
            case 'Q':
                right = CASTLE_WHITE_QUEENSIDE;
                break;
            case 'k':
                right = CASTLE_BLACK_KINGSIDE;
                break;
            case 'q':
                right = CASTLE_BLACK_QUEENSIDE;
                break;
            default:
                return -1;
            }

            if ((rights & right) != 0)
            {
                return -1;
            }

            rights |= right;
            ++p;
        }
    }

    if (*p != ' ')
    {
        return -1;
    }

    board->castling_rights = rights;
    *cursor = p + 1;
    return 0;
}

static int parse_square_token(const char *token, Square *square)
{
    int file;
    int rank;

    if (token[0] < 'a' || token[0] > 'h' || token[1] < '1' || token[1] > '8')
    {
        return -1;
    }

    file = token[0] - 'a';
    rank = token[1] - '1';
    *square = (Square)((rank * 8) + file);
    return 0;
}

static int parse_fen_en_passant(Board *board, const char **cursor)
{
    const char *p = *cursor;

    if (p[0] == '-' && p[1] == ' ')
    {
        board->en_passant_square = SQUARE_COUNT;
        *cursor = p + 2;
        return 0;
    }

    if (parse_square_token(p, &board->en_passant_square) != 0 || p[2] != ' ')
    {
        return -1;
    }

    *cursor = p + 3;
    return 0;
}

static int parse_non_negative_int(const char **cursor, int *value, int require_trailing_space)
{
    char *end;
    long parsed;
    const char *p = *cursor;

    if (!isdigit((unsigned char)*p))
    {
        return -1;
    }

    parsed = strtol(p, &end, 10);
    if (parsed < 0 || parsed > INT_MAX)
    {
        return -1;
    }

    if (require_trailing_space)
    {
        if (*end != ' ')
        {
            return -1;
        }
        *cursor = end + 1;
    }
    else
    {
        if (*end != '\0')
        {
            return -1;
        }
        *cursor = end;
    }

    *value = (int)parsed;
    return 0;
}

static int validate_king_positions(const Board *board)
{
    unsigned king_counts[COLOR_COUNT] = {0};

    for (int square = 0; square < SQUARE_COUNT; ++square)
    {
        Piece piece = board->squares[square];

        if (!piece_is_empty(piece) && piece_get_type(piece) == KING)
        {
            ++king_counts[piece_get_color(piece)];
        }
    }

    for (Color color = WHITE; color < COLOR_COUNT; ++color)
    {
        Square king_square = board->king_squares[color];

        if (king_counts[color] != 1 || king_square < A1 || king_square >= SQUARE_COUNT)
        {
            return -1;
        }

        if (board->squares[king_square] != make_piece(color, KING))
        {
            return -1;
        }
    }

    return 0;
}

static int validate_pawn_placement(const Board *board)
{
    for (int file = 0; file < BOARD_AXIS_LENGTH; ++file)
    {
        if (piece_get_type(board->squares[file]) == PAWN ||
            piece_get_type(board->squares[A8 + file]) == PAWN)
        {
            return -1;
        }
    }

    return 0;
}

static int validate_castling_rights(const Board *board)
{
    if ((board->castling_rights & CASTLE_WHITE_KINGSIDE) != 0 &&
        (board->squares[E1] != make_piece(WHITE, KING) ||
         board->squares[H1] != make_piece(WHITE, ROOK)))
    {
        return -1;
    }

    if ((board->castling_rights & CASTLE_WHITE_QUEENSIDE) != 0 &&
        (board->squares[E1] != make_piece(WHITE, KING) ||
         board->squares[A1] != make_piece(WHITE, ROOK)))
    {
        return -1;
    }

    if ((board->castling_rights & CASTLE_BLACK_KINGSIDE) != 0 &&
        (board->squares[E8] != make_piece(BLACK, KING) ||
         board->squares[H8] != make_piece(BLACK, ROOK)))
    {
        return -1;
    }

    if ((board->castling_rights & CASTLE_BLACK_QUEENSIDE) != 0 &&
        (board->squares[E8] != make_piece(BLACK, KING) ||
         board->squares[A8] != make_piece(BLACK, ROOK)))
    {
        return -1;
    }

    return 0;
}

static int validate_en_passant_square(const Board *board)
{
    Piece expected_moved_pawn;
    Square en_passant_square;
    Square moved_pawn_square;
    int expected_rank;

    if (board->en_passant_square == SQUARE_COUNT)
    {
        return 0;
    }

    en_passant_square = board->en_passant_square;
    expected_rank = board->side_to_move == WHITE ? 5 : 2;
    if (square_rank(en_passant_square) != expected_rank)
    {
        return -1;
    }

    if (!piece_is_empty(board->squares[en_passant_square]))
    {
        return -1;
    }

    if (board->side_to_move == WHITE)
    {
        moved_pawn_square = (Square)(en_passant_square - BOARD_AXIS_LENGTH);
        expected_moved_pawn = make_piece(BLACK, PAWN);
    }
    else
    {
        moved_pawn_square = (Square)(en_passant_square + BOARD_AXIS_LENGTH);
        expected_moved_pawn = make_piece(WHITE, PAWN);
    }

    if (board->squares[moved_pawn_square] != expected_moved_pawn)
    {
        return -1;
    }

    return 0;
}

static int validate_check_state(const Board *board)
{
    Color side_not_to_move = board_opposite_color(board->side_to_move);
    int white_in_check = move_gen_is_square_attacked(board, board->king_squares[WHITE], BLACK);
    int black_in_check = move_gen_is_square_attacked(board, board->king_squares[BLACK], WHITE);

    if (white_in_check && black_in_check)
    {
        return -1;
    }

    if ((side_not_to_move == WHITE && white_in_check) ||
        (side_not_to_move == BLACK && black_in_check))
    {
        return -1;
    }

    return 0;
}

static int validate_fen_position(const Board *board)
{
    if (validate_king_positions(board) != 0)
    {
        return -1;
    }

    if (validate_pawn_placement(board) != 0)
    {
        return -1;
    }

    if (validate_castling_rights(board) != 0)
    {
        return -1;
    }

    if (validate_en_passant_square(board) != 0)
    {
        return -1;
    }

    return validate_check_state(board);
}

int board_from_fen(Board *board, const char *fen)
{
    const char *cursor = fen;

    if (board == NULL || fen == NULL)
    {
        return -1;
    }

    board_clear(board);

    if (parse_fen_board(board, &cursor) != 0)
    {
        return -1;
    }

    if (parse_fen_side_to_move(board, &cursor) != 0)
    {
        return -1;
    }

    if (parse_fen_castling(board, &cursor) != 0)
    {
        return -1;
    }

    if (parse_fen_en_passant(board, &cursor) != 0)
    {
        return -1;
    }

    if (parse_non_negative_int(&cursor, &board->halfmove_clock, 1) != 0)
    {
        return -1;
    }

    if (parse_non_negative_int(&cursor, &board->fullmove_number, 0) != 0 ||
        board->fullmove_number == 0)
    {
        return -1;
    }

    if (validate_fen_position(board) != 0)
    {
        return -1;
    }

    board->position_key = zobrist_compute_key(board);

    return 0;
}