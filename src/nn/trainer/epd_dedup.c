#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "boards.h"
#include "fen.h"
#include "zobrist.h"

// Hash table configuration: 1<<20 buckets (approx 1M) for ~120k records
#define HASH_SIZE (1 << 20)

typedef struct HashEntry {
    uint64_t hashKey;
    char *fen;
    struct HashEntry *next;
} HashEntry;

static HashEntry *hashTable[HASH_SIZE];

// Helper to extract the FEN portion of the EPD line by truncating at "score" or ";"
static void extract_fen(const char *line, char *fen_out, size_t max_len) {
    char line_copy[1024];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';

    char *ptr = strstr(line_copy, "score");
    if (!ptr) {
        ptr = strchr(line_copy, ';');
    }
    if (ptr) {
        *ptr = '\0';
    }

    // Trim trailing whitespace and semicolons
    int len = (int)strlen(line_copy);
    while (len > 0 && (line_copy[len - 1] == ' ' || line_copy[len - 1] == ';' || 
                       line_copy[len - 1] == '\t' || line_copy[len - 1] == '\r' || 
                       line_copy[len - 1] == '\n')) {
        line_copy[len - 1] = '\0';
        len--;
    }

    strncpy(fen_out, line_copy, max_len - 1);
    fen_out[max_len - 1] = '\0';
}

// Truncate the FEN string at the 4th space to exclude halfmove clock and fullmove number
static void truncate_clocks(char *fen) {
    int space_count = 0;
    for (int i = 0; fen[i] != '\0'; ++i) {
        if (fen[i] == ' ') {
            space_count++;
            if (space_count == 4) {
                fen[i] = '\0';
                break;
            }
        }
    }
}

// Lookup and insert into the hash table
// Returns true if the position is a duplicate, false if it is new.
static bool check_and_insert(uint64_t hashKey, const char *fen) {
    uint32_t bucket = (uint32_t)(hashKey % HASH_SIZE);
    HashEntry *entry = hashTable[bucket];
    
    while (entry != NULL) {
        if (entry->hashKey == hashKey) {
            if (strcmp(entry->fen, fen) == 0) {
                return true; // Match found! It's a duplicate.
            }
        }
        entry = entry->next;
    }
    
    // Position not found, insert a new entry
    HashEntry *new_entry = malloc(sizeof(HashEntry));
    if (!new_entry) {
        fprintf(stderr, "Error: Out of memory allocating hash entry\n");
        exit(1);
    }
    new_entry->hashKey = hashKey;
    new_entry->fen = strdup(fen);
    new_entry->next = hashTable[bucket];
    hashTable[bucket] = new_entry;
    
    return false;
}

static void free_hash_table(void) {
    for (int i = 0; i < HASH_SIZE; ++i) {
        HashEntry *entry = hashTable[i];
        while (entry != NULL) {
            HashEntry *next = entry->next;
            free(entry->fen);
            free(entry);
            entry = next;
        }
        hashTable[i] = NULL;
    }
}

static void print_help(const char *prog_name) {
    printf("ZeroG EPD Deduplication Tool\n");
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -i, --input <file>     Input EPD file (default: quiet_training_positions_evaluated.epd)\n");
    printf("  -o, --output <file>    Output EPD file (default: quiet_training_positions_evaluated_dedup.epd)\n");
    printf("  -h, --help             Display this help and exit\n");
}

int main(int argc, char **argv) {
    const char *input_path = "quiet_training_positions_evaluated.epd";
    const char *output_path = "quiet_training_positions_evaluated_dedup.epd";
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0) {
            if (i + 1 < argc) input_path = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_path = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s. Use -h or --help for details.\n", argv[i]);
            return 1;
        }
    }
    
    // Initialize the engine structures needed by the FEN parser and Zobrist hash key generator
    bitboard_init();
    zobrist_init();
    
    printf("Reading EPD positions from: %s\n", input_path);
    FILE *in_f = fopen(input_path, "r");
    if (!in_f) {
        fprintf(stderr, "Error: Could not open input file '%s'\n", input_path);
        return 1;
    }
    
    FILE *out_f = fopen(output_path, "w");
    if (!out_f) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", output_path);
        fclose(in_f);
        return 1;
    }
    
    clock_t start_time = clock();
    
    char line[1024];
    int total_lines = 0;
    int parsed_count = 0;
    int dup_count = 0;
    int error_count = 0;
    int comment_skip_count = 0;
    
    char fen[256];
    char normalized_fen[256];
    Position pos;
    
    while (fgets(line, sizeof(line), in_f)) {
        total_lines++;
        
        // Skip comment lines or empty lines
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') {
            comment_skip_count++;
            continue;
        }
        
        // 1. Extract the raw FEN substring from EPD line
        extract_fen(line, fen, sizeof(fen));
        
        // 2. Parse FEN into Position struct
        if (fen_parse(fen, &pos) != 0) {
            // Write it out but log error
            error_count++;
            fprintf(stderr, "Warning: Failed to parse FEN at line %d: %s\n", total_lines, fen);
            fputs(line, out_f);
            continue;
        }
        
        parsed_count++;
        
        // 3. Serialize to normalized FEN & truncate clocks
        fen_serialize(&pos, normalized_fen);
        truncate_clocks(normalized_fen);
        
        // 4. Check for duplicates in hash table using Zobrist hash key and normalized FEN
        bool is_dup = check_and_insert(pos.hashKey, normalized_fen);
        
        if (is_dup) {
            dup_count++;
        } else {
            // Write unique line to output EPD
            fputs(line, out_f);
        }
        
        if (parsed_count % 20000 == 0) {
            printf("Processed: %d lines | Duplicates: %d\n", parsed_count, dup_count);
        }
    }
    
    fclose(in_f);
    fclose(out_f);
    
    double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    
    printf("\nDeduplication summary:\n");
    printf("----------------------------------------------------------\n");
    printf("Total lines read:        %d\n", total_lines);
    printf("Positions parsed:        %d\n", parsed_count);
    printf("Duplicates removed:      %d (%.2f%%)\n", dup_count, (double)dup_count / parsed_count * 100.0);
    printf("Errors / Parse failures: %d\n", error_count);
    printf("Empty / Comment lines:   %d\n", comment_skip_count);
    printf("Unique positions output: %d\n", parsed_count - dup_count);
    printf("Execution time:          %.3f seconds\n", elapsed);
    printf("Output file written to:  %s\n", output_path);
    printf("----------------------------------------------------------\n");
    
    free_hash_table();
    return 0;
}
