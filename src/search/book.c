#include "search/book.h"
#include "search/polyglot_keys.h"
#include "movegen/movegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int use_book = 0;
char book_path[512] = "book.bin";

/* Helper to decode big-endian uint64_t from byte stream */
static inline uint64_t decode_u64(const uint8_t *b) {
    return ((uint64_t)b[0] << 56) |
           ((uint64_t)b[1] << 48) |
           ((uint64_t)b[2] << 40) |
           ((uint64_t)b[3] << 32) |
           ((uint64_t)b[4] << 24) |
           ((uint64_t)b[5] << 16) |
           ((uint64_t)b[6] << 8)  |
           ((uint64_t)b[7]);
}

/* Helper to decode big-endian uint16_t from byte stream */
static inline uint16_t decode_u16(const uint8_t *b) {
    return ((uint16_t)b[0] << 8) |
           ((uint16_t)b[1]);
}

uint64_t book_polyglot_hash(const Position *board) {
    uint64_t hash = 0;

    /* 1. Pieces on squares */
    for (int sq = 0; sq < SQUARE_NB; ++sq) {
        Piece p = board->board[sq];
        if (p != EMPTY) {
            PieceType pt = PIECE_TYPE(p);
            Color c = PIECE_COLOR(p);
            /* Polyglot piece encoding: Black Pawn = 0, White Pawn = 1, Black Knight = 2... */
            int polyglot_piece = (pt - 1) * 2 + (c == WHITE ? 1 : 0);
            hash ^= POLYGLOT_RANDOM_ARRAY[polyglot_piece * 64 + sq];
        }
    }

    /* 2. Castling rights */
    if (board->castlingRights & CASTLE_WK) hash ^= POLYGLOT_RANDOM_ARRAY[768];
    if (board->castlingRights & CASTLE_WQ) hash ^= POLYGLOT_RANDOM_ARRAY[769];
    if (board->castlingRights & CASTLE_BK) hash ^= POLYGLOT_RANDOM_ARRAY[770];
    if (board->castlingRights & CASTLE_BQ) hash ^= POLYGLOT_RANDOM_ARRAY[771];

    /* 3. En passant file (only if an opposing pawn can capture) */
    int ep_sq = board->enPassantSquare;
    if (ep_sq >= A1 && ep_sq < SQUARE_NB) {
        int file = FILE_OF(ep_sq);
        int has_pawn = 0;
        Color us = board->sideToMove;
        Piece our_pawn = MAKE_PIECE(us, PAWN);

        if (us == WHITE) {
            /* White captures en passant. White pawns must be on Rank 5 to reach Rank 6.
             * Adjacent files are file - 1 (sq - 9) and file + 1 (sq - 7). */
            if (file > FILE_A && board->board[ep_sq - 9] == our_pawn) has_pawn = 1;
            if (file < FILE_H && board->board[ep_sq - 7] == our_pawn) has_pawn = 1;
        } else {
            /* Black captures en passant. Black pawns must be on Rank 4 to reach Rank 3.
             * Adjacent files are file - 1 (sq + 7) and file + 1 (sq + 9). */
            if (file > FILE_A && board->board[ep_sq + 7] == our_pawn) has_pawn = 1;
            if (file < FILE_H && board->board[ep_sq + 9] == our_pawn) has_pawn = 1;
        }

        if (has_pawn) {
            hash ^= POLYGLOT_RANDOM_ARRAY[772 + file];
        }
    }

    /* 4. Side to move (only if White is to move) */
    if (board->sideToMove == WHITE) {
        hash ^= POLYGLOT_RANDOM_ARRAY[780];
    }

    return hash;
}

typedef struct {
    uint8_t key_bytes[8];
    uint8_t move_bytes[2];
    uint8_t weight_bytes[2];
    uint8_t learn_bytes[4];
} RawBookEntry;

typedef struct {
    uint16_t raw_move;
    uint16_t weight;
} BookMoveCandidate;

Move book_probe(const Position *board) {
    if (!use_book) {
        return 0;
    }

    if (book_path[0] == '\0') {
        fprintf(stderr, "Error: OwnBook option is enabled but BookFile is empty.\n");
        printf("info string Error: OwnBook enabled but BookFile is empty\n");
        fflush(stdout);
        exit(1);
    }

    FILE *file = fopen(book_path, "rb");
    if (!file) {
        fprintf(stderr, "Error: OwnBook option is enabled but book file could not be opened: %s\n", book_path);
        printf("info string Error: OwnBook enabled but book file could not be opened: %s\n", book_path);
        fflush(stdout);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    long num_entries = file_size / 16;
    if (num_entries <= 0) {
        fclose(file);
        return 0;
    }

    uint64_t target_key = book_polyglot_hash(board);
    long low = 0;
    long high = num_entries - 1;
    long first_match = -1;

    /* Binary search for the target key */
    while (low <= high) {
        long mid = low + (high - low) / 2;
        if (fseek(file, mid * 16, SEEK_SET) != 0) {
            break;
        }

        RawBookEntry entry;
        if (fread(&entry, sizeof(RawBookEntry), 1, file) != 1) {
            break;
        }

        uint64_t mid_key = decode_u64(entry.key_bytes);
        if (mid_key == target_key) {
            first_match = mid;
            high = mid - 1; /* Find the first entry with this key */
        } else if (mid_key < target_key) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    if (first_match == -1) {
        fclose(file);
        return 0;
    }

    /* Gather all matching candidates */
    BookMoveCandidate candidates[32];
    int candidate_count = 0;
    uint32_t total_weight = 0;

    if (fseek(file, first_match * 16, SEEK_SET) == 0) {
        while (candidate_count < 32) {
            RawBookEntry entry;
            if (fread(&entry, sizeof(RawBookEntry), 1, file) != 1) {
                break;
            }

            uint64_t entry_key = decode_u64(entry.key_bytes);
            if (entry_key != target_key) {
                break;
            }

            uint16_t raw_move = decode_u16(entry.move_bytes);
            uint16_t weight = decode_u16(entry.weight_bytes);

            candidates[candidate_count].raw_move = raw_move;
            candidates[candidate_count].weight = weight;
            total_weight += weight;
            candidate_count++;
        }
    }

    fclose(file);

    if (candidate_count == 0) {
        return 0;
    }

    /* Select a candidate move */
    uint16_t selected_raw_move = 0;
    if (total_weight > 0) {
        /* Weighted random selection */
        uint32_t r = rand() % total_weight;
        uint32_t running_sum = 0;
        for (int i = 0; i < candidate_count; ++i) {
            running_sum += candidates[i].weight;
            if (r < running_sum) {
                selected_raw_move = candidates[i].raw_move;
                break;
            }
        }
    } else {
        /* Uniform random selection as fallback */
        selected_raw_move = candidates[rand() % candidate_count].raw_move;
    }

    if (selected_raw_move == 0) {
        return 0;
    }

    /* Decode Polyglot move representation:
     * bits 0-5  : to square
     * bits 6-11 : from square
     * bits 12-14: promotion type (0=None, 1=Knight, 2=Bishop, 3=Rook, 4=Queen) */
    Square from = (selected_raw_move >> 6) & 0x3F;
    Square to = selected_raw_move & 0x3F;
    int promo_type = (selected_raw_move >> 12) & 7;

    /* Polyglot represents castling as king captures rook.
     * Translate to standard king destinations. */
    if (from == E1 && to == H1 && board->board[E1] == W_KING) {
        to = G1;
    } else if (from == E1 && to == A1 && board->board[E1] == W_KING) {
        to = C1;
    } else if (from == E8 && to == H8 && board->board[E8] == B_KING) {
        to = G8;
    } else if (from == E8 && to == A8 && board->board[E8] == B_KING) {
        to = C8;
    }

    int promotion_type = -1;
    if (promo_type == 1) promotion_type = 0;      /* Knight */
    else if (promo_type == 2) promotion_type = 1; /* Bishop */
    else if (promo_type == 3) promotion_type = 2; /* Rook */
    else if (promo_type == 4) promotion_type = 3; /* Queen */

    /* Generate all legal moves to find the matching engine Move structure */
    Move legal_moves[MAX_MOVES];
    int count = movegen_legal(board, legal_moves);
    for (int i = 0; i < count; ++i) {
        Move candidate = legal_moves[i];
        if (MOVE_FROM(candidate) == from && MOVE_TO(candidate) == to) {
            Piece p = board->board[from];
            PieceType pt = PIECE_TYPE(p);
            int is_candidate_promo = (pt == PAWN) && (RANK_OF(to) == RANK_1 || RANK_OF(to) == RANK_8);

            if (is_candidate_promo) {
                if (MOVE_PROMO(candidate) == promotion_type) {
                    return candidate;
                }
            } else {
                if (promotion_type == -1) {
                    return candidate;
                }
            }
        }
    }

    return 0; /* No legal move matched */
}
