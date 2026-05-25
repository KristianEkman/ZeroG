#include "fen.h"
#include "boards.h"
#include "zobrist.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * FEN parsing
 * ──────────────────────────────────────────────────────────────────────────── */

/* Map a piece character to (Color, PieceType).  Returns 0 on success. */
static int char_to_piece(char ch, Color *color, PieceType *pt)
{
    switch (ch) {
        case 'P': *color = WHITE; *pt = PAWN;   return 0;
        case 'N': *color = WHITE; *pt = KNIGHT; return 0;
        case 'B': *color = WHITE; *pt = BISHOP; return 0;
        case 'R': *color = WHITE; *pt = ROOK;   return 0;
        case 'Q': *color = WHITE; *pt = QUEEN;  return 0;
        case 'K': *color = WHITE; *pt = KING;   return 0;
        case 'p': *color = BLACK; *pt = PAWN;   return 0;
        case 'n': *color = BLACK; *pt = KNIGHT; return 0;
        case 'b': *color = BLACK; *pt = BISHOP; return 0;
        case 'r': *color = BLACK; *pt = ROOK;   return 0;
        case 'q': *color = BLACK; *pt = QUEEN;  return 0;
        case 'k': *color = BLACK; *pt = KING;   return 0;
        default:  return -1;
    }
}

int fen_parse(const char *fen, Position *pos)
{
    if (!fen || !pos) return -1;

    /* Zero the position first */
    memset(pos, 0, sizeof(Position));

    const char *p = fen;

    /* ── 1. Piece placement ─────────────────────────────────────────── */
    int file = 0, rank = 7;
    while (rank >= 0) {
        if (file == 8) {
            if (rank == 0) break;                   /* last rank done */
            if (*p != '/') return -1;               /* missing rank separator */
            p++;
            rank--;
            file = 0;
            continue;
        }
        if (*p >= '1' && *p <= '8') {
            int empty = *p - '0';
            if (file + empty > 8) return -1;
            for (int i = 0; i < empty; i++) {
                int sq = SQUARE(file + i, rank);
                pos->board[sq] = EMPTY;
            }
            file += empty;
            p++;
            continue;
        }
        Color color;
        PieceType pt;
        if (char_to_piece(*p, &color, &pt) != 0)
            return -1;
        int sq = SQUARE(file, rank);
        Piece piece = MAKE_PIECE(color, pt);
        pos->board[sq] = piece;
        pos->pieces[COLOR_IDX(color)][pt] |= (1ULL << sq);
        pos->occ[COLOR_IDX(color)]      |= (1ULL << sq);
        if (pt == KING)
            pos->kingSq[COLOR_IDX(color)] = sq;
        file++;
        p++;
    }
    if (file != 8 || rank != 0) return -1;

    pos->occAll = pos->occ[COLOR_IDX(WHITE)] | pos->occ[COLOR_IDX(BLACK)];

    /* ── 2. Side to move ────────────────────────────────────────────── */
    if (*p != ' ') return -1;
    p++;
    if (*p == 'w')
        pos->sideToMove = WHITE;
    else if (*p == 'b')
        pos->sideToMove = BLACK;
    else
        return -1;
    p++;

    /* ── 3. Castling rights ─────────────────────────────────────────── */
    if (*p != ' ') return -1;
    p++;
    pos->castlingRights = 0;
    if (*p == '-') {
        p++;
    } else {
        while (*p && *p != ' ') {
            switch (*p) {
                case 'K': pos->castlingRights |= CASTLE_WK; break;
                case 'Q': pos->castlingRights |= CASTLE_WQ; break;
                case 'k': pos->castlingRights |= CASTLE_BK; break;
                case 'q': pos->castlingRights |= CASTLE_BQ; break;
                default:  return -1;
            }
            p++;
        }
    }

    /* ── 4. En passant square ───────────────────────────────────────── */
    if (*p != ' ') return -1;
    p++;
    if (*p == '-') {
        pos->enPassantSquare = SQ_NONE;
        p++;
    } else if (*p >= 'a' && *p <= 'h' && *(p+1) >= '1' && *(p+1) <= '8') {
        int f = *p - 'a';
        int r = *(p+1) - '1';
        pos->enPassantSquare = SQUARE(f, r);
        p += 2;
    } else {
        return -1;
    }

    /* ── 5. Halfmove clock ──────────────────────────────────────────── */
    if (*p == '\0') {
        pos->fiftyMoveCounter = 0;
        pos->fullmoveNumber = 1;
        pos->hashKey = zobrist_compute_key(pos);
        return 0;
    }

    if (*p != ' ') return -1;
    p++;
    pos->fiftyMoveCounter = 0;
    while (*p >= '0' && *p <= '9') {
        pos->fiftyMoveCounter = pos->fiftyMoveCounter * 10 + (int)(*p - '0');
        p++;
    }

    if (*p != ' ') return -1;
    p++;
    pos->fullmoveNumber = 0;
    while (*p >= '0' && *p <= '9') {
        pos->fullmoveNumber = pos->fullmoveNumber * 10 + (int)(*p - '0');
        p++;
    }
    if (pos->fullmoveNumber < 1) {
        pos->fullmoveNumber = 1;
    }

    pos->hashKey = zobrist_compute_key(pos);
    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * FEN serialisation
 * ──────────────────────────────────────────────────────────────────────────── */

static char piece_to_char(Piece p)
{
    static const char ch[] = { '?', 'P', 'N', 'B', 'R', 'Q', 'K' };
    PieceType pt = PIECE_TYPE(p);
    if (pt < PAWN || pt > KING) return '?';
    char c = ch[pt];
    return (PIECE_COLOR(p) == BLACK) ? (char)tolower((unsigned char)c) : c;
}

int fen_serialize(const Position *pos, char *buf)
{
    if (!pos || !buf) return -1;

    char *out = buf;

    /* ── 1. Piece placement ─────────────────────────────────────────── */
    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            Piece p = pos->board[rank * 8 + file];
            if (p == EMPTY) {
                empty++;
            } else {
                if (empty > 0) {
                    *out++ = (char)('0' + empty);
                    empty = 0;
                }
                *out++ = piece_to_char(p);
            }
        }
        if (empty > 0) {
            *out++ = (char)('0' + empty);
        }
        if (rank > 0)
            *out++ = '/';
    }

    /* ── 2. Side to move ────────────────────────────────────────────── */
    *out++ = ' ';
    *out++ = (pos->sideToMove == WHITE) ? 'w' : 'b';

    /* ── 3. Castling rights ─────────────────────────────────────────── */
    *out++ = ' ';
    if (pos->castlingRights == 0) {
        *out++ = '-';
    } else {
        if (pos->castlingRights & CASTLE_WK) *out++ = 'K';
        if (pos->castlingRights & CASTLE_WQ) *out++ = 'Q';
        if (pos->castlingRights & CASTLE_BK) *out++ = 'k';
        if (pos->castlingRights & CASTLE_BQ) *out++ = 'q';
    }

    /* ── 4. En passant square ───────────────────────────────────────── */
    *out++ = ' ';
    if (pos->enPassantSquare == SQ_NONE) {
        *out++ = '-';
    } else {
        *out++ = (char)('a' + FILE_OF(pos->enPassantSquare));
        *out++ = (char)('1' + RANK_OF(pos->enPassantSquare));
    }

    /* ── 5. Halfmove clock ──────────────────────────────────────────── */
    out += sprintf(out, " %d", pos->fiftyMoveCounter);

    /* ── 6. Fullmove number ──────────────────────────────────────────── */
    out += sprintf(out, " %d", pos->fullmoveNumber);

    *out = '\0';
    return (int)(out - buf);
}