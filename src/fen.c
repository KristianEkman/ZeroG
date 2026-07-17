#include "fen.h"
#include "boards.h"
#include "zobrist.h"
#include "movegen.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

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
    } else {
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
    }

    /* Check that other side to move king is not in check */
    Color other_side = OPPOSITE(pos->sideToMove);
    int other_king_sq = pos->kingSq[COLOR_IDX(other_side)];
    if (pos->board[other_king_sq] == MAKE_PIECE(other_side, KING)) {
        if (is_square_attacked(pos, other_king_sq, pos->sideToMove)) {
            return -1;
        }
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

static bool is_number_token(const char *s, const char *end) {
    if (s == end) return false;
    for (const char *ptr = s; ptr < end; ptr++) {
        if (!isdigit((unsigned char)*ptr)) return false;
    }
    return true;
}

int parse_epd_line(const char *line, Position *pos, float *target) {
    char line_copy[1024];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';
    
    // Find the end of the FEN part and the start of the opcodes.
    // Standard FEN contains 4 required fields, and optionally 2 clock/move fields.
    const char *p = line;
    
    // Skip any leading whitespace
    while (*p && isspace((unsigned char)*p)) p++;
    
    // Skip 4 FEN fields
    for (int i = 0; i < 4; i++) {
        const char *f_start = p;
        while (*p && !isspace((unsigned char)*p)) p++; // skip non-space
        const char *f_end = p;
        if (f_start == f_end) {
            return -4; // Invalid FEN
        }
        while (*p && isspace((unsigned char)*p)) p++;  // skip space
    }
    
    const char *fen_end = p;
    
    // Check if the next token is field 5 (halfmove clock)
    const char *next = p;
    if (*next) {
        const char *f5_start = next;
        while (*next && !isspace((unsigned char)*next)) next++;
        const char *f5_end = next;
        
        // If it consists entirely of digits, it's the halfmove clock
        if (is_number_token(f5_start, f5_end)) {
            fen_end = f5_end;
            p = next;
            while (*p && isspace((unsigned char)*p)) p++;  // skip space
            
            // Check if the token after that is field 6 (fullmove number)
            next = p;
            if (*next) {
                const char *f6_start = next;
                while (*next && !isspace((unsigned char)*next)) next++;
                const char *f6_end = next;
                if (is_number_token(f6_start, f6_end)) {
                    fen_end = f6_end;
                    p = next;
                    while (*p && isspace((unsigned char)*p)) p++;  // skip space
                }
            }
        }
    }
    
    // Now p points to the start of the opcode section.
    // The FEN string is line_copy up to (fen_end - line).
    int fen_len = (int)(fen_end - line);
    if (fen_len >= (int)sizeof(line_copy)) {
        return -4;
    }
    
    // We will parse opcodes from p onwards.
    const char *opcode_ptr = p;
    const char *score_value_start = NULL;
    
    // Find the "score" opcode in the opcode section
    while (*opcode_ptr) {
        while (*opcode_ptr && isspace((unsigned char)*opcode_ptr)) opcode_ptr++;
        if (!*opcode_ptr) break;
        
        const char *op_name_start = opcode_ptr;
        // Opcode name is alphanumeric/underscore/hyphen
        while (*opcode_ptr && (isalnum((unsigned char)*opcode_ptr) || *opcode_ptr == '_' || *opcode_ptr == '-')) {
            opcode_ptr++;
        }
        const char *op_name_end = opcode_ptr;
        if (op_name_start == op_name_end) {
            // Invalid character at the start of an opcode name
            opcode_ptr++;
            continue;
        }
        
        // Skip whitespace between opcode and value
        while (*opcode_ptr && isspace((unsigned char)*opcode_ptr)) opcode_ptr++;
        
        const char *val_start = opcode_ptr;
        
        if (*opcode_ptr == '"') {
            // String value
            opcode_ptr++; // skip opening quote
            while (*opcode_ptr && *opcode_ptr != '"') {
                opcode_ptr++;
            }
            if (*opcode_ptr == '"') {
                opcode_ptr++; // skip closing quote
            }
        } else {
            // Read until semicolon or whitespace
            while (*opcode_ptr && *opcode_ptr != ';' && !isspace((unsigned char)*opcode_ptr)) {
                opcode_ptr++;
            }
            if (*opcode_ptr == ';') {
                opcode_ptr++;
            }
        }
        
        // Check if this opcode is "score"
        int op_len = (int)(op_name_end - op_name_start);
        if (op_len == 5 && strncmp(op_name_start, "score", 5) == 0) {
            score_value_start = val_start;
            break; // We found the score opcode!
        }
    }
    
    if (!score_value_start) {
        return -1; // No score opcode found
    }
    
    // Parse using strtol and validate
    char *value_end = NULL;
    errno = 0;
    long score = strtol(score_value_start, &value_end, 10);
    
    if (score_value_start == value_end || errno == ERANGE || score < INT_MIN || score > INT_MAX) {
        // If the value was not a valid integer, check if it's "mate"
        // standard mate skip logic
        char val_buf[64];
        int val_len = 0;
        const char *chk = score_value_start;
        while (*chk && *chk != ';' && !isspace((unsigned char)*chk) && val_len < (int)sizeof(val_buf) - 1) {
            val_buf[val_len++] = *chk++;
        }
        val_buf[val_len] = '\0';
        if (strstr(val_buf, "mate") || strchr(val_buf, 'M') || strchr(val_buf, 'm')) {
            return -2;
        }
        return -5;
    }
    
    while (isspace((unsigned char)*value_end)) {
        value_end++;
    }
    
    if (*value_end != '\0' && *value_end != ';') {
        return -5;
    }
    
    int score_val = (int)score;
    
    // Skip extreme positions
    if (score_val >= 1000 || score_val <= -1000) {
        return -3;
    }
    
    // Target is scaled from centipawns to pawn units
    *target = (float)score_val / 100.0f;
    
    // Prepare the FEN part for parsing
    // FEN part is line_copy up to fen_len
    line_copy[fen_len] = '\0';
    
    // Trim trailing spaces and semicolons from the FEN
    char *end = line_copy + fen_len - 1;
    while (end >= line_copy && (*end == ' ' || *end == ';' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    
    // Parse the FEN into Position
    if (fen_parse(line_copy, pos) != 0) {
        return -4;
    }
    
    return 0;
}