#include "transposition_table.h"

#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

enum {
    TRANSPOSITION_TABLE_BUCKET_SIZE = 4,
    TRANSPOSITION_TABLE_MATE_SCORE_THRESHOLD = 28000
};

typedef struct {
    _Atomic uint64_t key;
    Move best_move;
    int score;
    uint16_t depth;
    uint16_t generation;
    uint8_t bound;
    uint8_t occupied;
} TranspositionTableSlot;

static inline uint64_t transposition_table_hash_data(Move best_move, int score, uint16_t depth, uint16_t generation, uint8_t bound, uint8_t occupied) {
    uint64_t h = ((uint64_t)best_move) ^
                 (((uint64_t)score) << 16) ^
                 (((uint64_t)depth) << 48) ^
                 (((uint64_t)generation) << 32) ^
                 (((uint64_t)bound) << 52) ^
                 (((uint64_t)occupied) << 60);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

static size_t transposition_table_floor_power_of_two(size_t value)
{
    size_t power_of_two = 1;

    while (power_of_two <= value / 2) {
        power_of_two <<= 1;
    }

    return power_of_two;
}

static size_t transposition_table_capacity(size_t size_bytes)
{
    size_t slots_per_bucket = TRANSPOSITION_TABLE_BUCKET_SIZE;
    size_t slot_count = size_bytes / sizeof(TranspositionTableSlot);
    size_t bucket_count = slot_count / slots_per_bucket;

    if (bucket_count == 0) {
        return 1;
    }

    return transposition_table_floor_power_of_two(bucket_count);
}

static TranspositionTableSlot *transposition_table_slots(TranspositionTable *table)
{
    if (table == NULL) {
        return NULL;
    }

    return (TranspositionTableSlot *)table->storage;
}

static const TranspositionTableSlot *transposition_table_slots_const(const TranspositionTable *table)
{
    if (table == NULL) {
        return NULL;
    }

    return (const TranspositionTableSlot *)table->storage;
}

static size_t transposition_table_bucket_index(const TranspositionTable *table, uint64_t key)
{
    if (table == NULL || table->bucket_count == 0) {
        return 0;
    }

    return (size_t)key & (table->bucket_count - 1);
}

static TranspositionTableSlot *transposition_table_bucket_slot(TranspositionTable *table,
                                                               size_t bucket_index,
                                                               size_t slot_index)
{
    TranspositionTableSlot *slots = transposition_table_slots(table);

    return &slots[(bucket_index * TRANSPOSITION_TABLE_BUCKET_SIZE) + slot_index];
}

static const TranspositionTableSlot *transposition_table_bucket_slot_const(
    const TranspositionTable *table,
    size_t bucket_index,
    size_t slot_index)
{
    const TranspositionTableSlot *slots = transposition_table_slots_const(table);

    return &slots[(bucket_index * TRANSPOSITION_TABLE_BUCKET_SIZE) + slot_index];
}

static int transposition_table_score_to_storage(int score, unsigned ply)
{
    if (score >= TRANSPOSITION_TABLE_MATE_SCORE_THRESHOLD) {
        return score + (int)ply;
    }

    if (score <= -TRANSPOSITION_TABLE_MATE_SCORE_THRESHOLD) {
        return score - (int)ply;
    }

    return score;
}

static int transposition_table_score_from_storage(int score, unsigned ply)
{
    if (score >= TRANSPOSITION_TABLE_MATE_SCORE_THRESHOLD) {
        return score - (int)ply;
    }

    if (score <= -TRANSPOSITION_TABLE_MATE_SCORE_THRESHOLD) {
        return score + (int)ply;
    }

    return score;
}

static TranspositionTableSlot *transposition_table_find_replacement_slot(TranspositionTable *table,
                                                                         uint64_t key)
{
    size_t bucket_index;
    TranspositionTableSlot *replacement_slot;
    unsigned replacement_age;

    if (table == NULL || table->storage == NULL || table->bucket_count == 0) {
        return NULL;
    }

    bucket_index = transposition_table_bucket_index(table, key);
    replacement_slot = transposition_table_bucket_slot(table, bucket_index, 0);
    replacement_age = (uint16_t)(table->generation - replacement_slot->generation);

    for (size_t slot_index = 0; slot_index < TRANSPOSITION_TABLE_BUCKET_SIZE; ++slot_index) {
        TranspositionTableSlot *slot = transposition_table_bucket_slot(table, bucket_index, slot_index);
        unsigned slot_age;

        uint64_t stored_key = atomic_load_explicit(&slot->key, memory_order_relaxed);
        uint64_t data_hash = transposition_table_hash_data(
            slot->best_move, slot->score,
            slot->depth, slot->generation,
            slot->bound, slot->occupied);
        uint64_t actual_key = stored_key ^ data_hash;

        if (!slot->occupied || actual_key == key) {
            return slot;
        }

        slot_age = (uint16_t)(table->generation - slot->generation);
        if (slot_age > replacement_age) {
            replacement_slot = slot;
            replacement_age = slot_age;
            continue;
        }

        if (slot_age == replacement_age && slot->depth < replacement_slot->depth) {
            replacement_slot = slot;
        }
    }

    return replacement_slot;
}

int transposition_table_init(TranspositionTable *table, size_t size_bytes)
{
    size_t bucket_count;
    size_t slot_count;
    TranspositionTableSlot *storage;

    if (table == NULL) {
        return -1;
    }

    bucket_count = transposition_table_capacity(size_bytes);
    slot_count = bucket_count * TRANSPOSITION_TABLE_BUCKET_SIZE;
    storage = calloc(slot_count, sizeof(TranspositionTableSlot));
    if (storage == NULL) {
        table->storage = NULL;
        table->bucket_count = 0;
        table->generation = 0;
        return -1;
    }

    table->storage = storage;
    table->bucket_count = bucket_count;
    table->generation = 0;
    return 0;
}

void transposition_table_free(TranspositionTable *table)
{
    if (table == NULL) {
        return;
    }

    free(table->storage);
    table->storage = NULL;
    table->bucket_count = 0;
    table->generation = 0;
}

void transposition_table_clear(TranspositionTable *table)
{
    size_t slot_count;

    if (table == NULL || table->storage == NULL) {
        return;
    }

    slot_count = table->bucket_count * TRANSPOSITION_TABLE_BUCKET_SIZE;
    memset(table->storage, 0, slot_count * sizeof(TranspositionTableSlot));
    table->generation = 0;
}

void transposition_table_next_generation(TranspositionTable *table)
{
    if (table == NULL) {
        return;
    }

    ++table->generation;
}

int transposition_table_store(TranspositionTable *table,
                              uint64_t key,
                              unsigned depth,
                              unsigned ply,
                              int score,
                              TranspositionBound bound,
                              Move best_move)
{
    TranspositionTableSlot *slot;

    if (table == NULL || table->storage == NULL || bound == TT_BOUND_NONE) {
        return -1;
    }

    slot = transposition_table_find_replacement_slot(table, key);
    if (slot == NULL) {
        return -1;
    }

    uint64_t stored_key = atomic_load_explicit(&slot->key, memory_order_relaxed);
    uint64_t data_hash = transposition_table_hash_data(
        slot->best_move, slot->score,
        slot->depth, slot->generation,
        slot->bound, slot->occupied);
    uint64_t actual_key = stored_key ^ data_hash;

    if (slot->occupied && actual_key == key) {
        if (best_move == 0) {
            best_move = slot->best_move;
        }

        if (depth < slot->depth) {
            slot->best_move = best_move;
            slot->generation = (uint16_t)table->generation;

            uint64_t new_hash = transposition_table_hash_data(
                slot->best_move, slot->score,
                slot->depth, slot->generation,
                slot->bound, slot->occupied);
            atomic_store_explicit(&slot->key, key ^ new_hash, memory_order_release);
            return 0;
        }
    }

    slot->best_move = best_move;
    slot->score = transposition_table_score_to_storage(score, ply);
    slot->depth = (uint16_t)depth;
    slot->generation = (uint16_t)table->generation;
    slot->bound = (uint8_t)bound;
    slot->occupied = 1;

    uint64_t new_hash = transposition_table_hash_data(
        slot->best_move, slot->score,
        slot->depth, slot->generation,
        slot->bound, slot->occupied);
    atomic_store_explicit(&slot->key, key ^ new_hash, memory_order_release);
    return 0;
}

int transposition_table_lookup(const TranspositionTable *table,
                               uint64_t key,
                               unsigned ply,
                               TranspositionEntry *entry)
{
    size_t bucket_index;

    if (table == NULL || table->storage == NULL || entry == NULL || table->bucket_count == 0) {
        return -1;
    }

    bucket_index = transposition_table_bucket_index(table, key);

    for (size_t slot_index = 0; slot_index < TRANSPOSITION_TABLE_BUCKET_SIZE; ++slot_index) {
        const TranspositionTableSlot *slot = transposition_table_bucket_slot_const(table,
                                                                                   bucket_index,
                                                                                   slot_index);

        uint64_t stored_key = atomic_load_explicit(&slot->key, memory_order_acquire);
        Move slot_best_move = slot->best_move;
        int slot_score = slot->score;
        uint16_t slot_depth = slot->depth;
        uint16_t slot_generation = slot->generation;
        uint8_t slot_bound = slot->bound;
        uint8_t slot_occupied = slot->occupied;

        uint64_t data_hash = transposition_table_hash_data(
            slot_best_move, slot_score,
            slot_depth, slot_generation,
            slot_bound, slot_occupied);

        if (!slot_occupied || (stored_key ^ data_hash) != key) {
            continue;
        }

        entry->key = key;
        entry->best_move = slot_best_move;
        entry->score = transposition_table_score_from_storage(slot_score, ply);
        entry->depth = slot_depth;
        entry->generation = slot_generation;
        entry->bound = (TranspositionBound)slot_bound;
        return 1;
    }

    return 0;
}