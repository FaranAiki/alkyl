#include "hashmap.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define HASHMAP_INIT_SIZE 256
#define PERTURB_SHIFT 5

typedef struct {
    uint32_t hash;
    const char *key;
    void *value;
} DictEntry;

// FNV-1a hash function
static uint32_t hash_string(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)(*str++);
        hash *= 16777619u;
    }
    return hash;
}

static void hashmap_resize(HashMap *map) {
    int old_cap = map->capacity;
    int new_cap = old_cap * 2;
    int new_limit = (new_cap * 2) / 3;

    int32_t *old_indices = (int32_t*)map->buckets;
    DictEntry *old_entries = (DictEntry*)(old_indices + old_cap);

    size_t new_bytes = new_cap * sizeof(int32_t) + new_limit * sizeof(DictEntry);
    void *new_block;
    
    if (map->arena) {
        new_block = arena_alloc(map->arena, new_bytes);
    } else {
        new_block = malloc(new_bytes);
    }

    int32_t *new_indices = (int32_t*)new_block;
    DictEntry *new_entries = (DictEntry*)(new_indices + new_cap);

    // Initialize all new sparse indices to -1 (Empty)
    for (int i = 0; i < new_cap; i++) {
        new_indices[i] = -1;
    }

    // 1. Copy the dense entries array directly (Extremely fast, preserves order)
    if (map->size > 0) {
        memcpy(new_entries, old_entries, map->size * sizeof(DictEntry));
    }

    // 2. Rebuild the sparse hash table using cached hashes
    size_t mask = new_cap - 1;
    for (int i = 0; i < map->size; i++) {
        uint32_t hash = new_entries[i].hash;
        size_t perturb = hash;
        size_t j = hash & mask;
        
        while (new_indices[j] != -1) {
            j = (j * 5 + 1 + perturb) & mask;
            perturb >>= PERTURB_SHIFT;
        }
        new_indices[j] = i;
    }

    if (!map->arena) {
        free(map->buckets);
    }
    
    map->capacity = new_cap;
    map->buckets = (MapEntry**)new_block;
}

void hashmap_init(HashMap *map, Arena *arena, int initial_capacity) {
    if (!map) return;
    
    int cap = HASHMAP_INIT_SIZE;
    if (initial_capacity > 0) {
        cap = 8; // Python dicts minimum
        while (cap < initial_capacity) {
            cap <<= 1;
        }
    }
    
    map->capacity = cap;
    map->size = 0;
    map->arena = arena;
    
    int limit = (cap * 2) / 3;
    size_t bytes = cap * sizeof(int32_t) + limit * sizeof(DictEntry);
    
    if (arena) {
        map->buckets = (MapEntry**)arena_alloc(arena, bytes);
    } else {
        map->buckets = (MapEntry**)malloc(bytes);
    }
    
    int32_t *indices = (int32_t*)map->buckets;
    for (int i = 0; i < cap; i++) {
        indices[i] = -1;
    }
}

void hashmap_put(HashMap *map, const char *key, void *value) {
    if (!map || !key) return;
    
    // Resize at 2/3 load factor (Python's threshold)
    int limit = (map->capacity * 2) / 3;
    if (map->size >= limit) {
        hashmap_resize(map);
    }
    
    uint32_t hash = hash_string(key);
    size_t mask = map->capacity - 1;
    size_t perturb = hash;
    size_t i = hash & mask;
    
    int32_t *indices = (int32_t*)map->buckets;
    DictEntry *entries = (DictEntry*)(indices + map->capacity);
    
    // Probing loop
    while (indices[i] != -1) {
        int idx = indices[i];
        // Pointer equality short-circuit is a huge win for compiler interned strings
        if (entries[idx].hash == hash && 
           (entries[idx].key == key || strcmp(entries[idx].key, key) == 0)) {
            entries[idx].value = value;
            return;
        }
        i = (i * 5 + 1 + perturb) & mask;
        perturb >>= PERTURB_SHIFT;
    }
    
    // Insert new entry
    int new_idx = map->size++;
    indices[i] = new_idx;
    
    entries[new_idx].hash = hash;
    if (map->arena) {
        entries[new_idx].key = arena_strdup(map->arena, key);
    } else {
        entries[new_idx].key = strdup(key);
    }
    entries[new_idx].value = value;
}

void* hashmap_get(HashMap *map, const char *key) {
    if (!map || !key) return NULL;
    
    uint32_t hash = hash_string(key);
    size_t mask = map->capacity - 1;
    size_t perturb = hash;
    size_t i = hash & mask;
    
    int32_t *indices = (int32_t*)map->buckets;
    DictEntry *entries = (DictEntry*)(indices + map->capacity);
    
    while (indices[i] != -1) {
        int idx = indices[i];
        if (entries[idx].hash == hash && 
           (entries[idx].key == key || strcmp(entries[idx].key, key) == 0)) {
            return entries[idx].value;
        }
        i = (i * 5 + 1 + perturb) & mask;
        perturb >>= PERTURB_SHIFT;
    }
    
    return NULL;
}

int hashmap_has(HashMap *map, const char *key) {
    return hashmap_get(map, key) != NULL;
}

int hashmap_inc(HashMap *map, const char *key) {
    if (!map || !key) return 0;
    
    int limit = (map->capacity * 2) / 3;
    if (map->size >= limit) {
        hashmap_resize(map);
    }
    
    uint32_t hash = hash_string(key);
    size_t mask = map->capacity - 1;
    size_t perturb = hash;
    size_t i = hash & mask;
    
    int32_t *indices = (int32_t*)map->buckets;
    DictEntry *entries = (DictEntry*)(indices + map->capacity);
    
    while (indices[i] != -1) {
        int idx = indices[i];
        if (entries[idx].hash == hash && 
           (entries[idx].key == key || strcmp(entries[idx].key, key) == 0)) {
            intptr_t count = (intptr_t)entries[idx].value;
            count++;
            entries[idx].value = (void*)count;
            return (int)count;
        }
        i = (i * 5 + 1 + perturb) & mask;
        perturb >>= PERTURB_SHIFT;
    }
    
    // Insert new entry with count 1
    int new_idx = map->size++;
    indices[i] = new_idx;
    
    entries[new_idx].hash = hash;
    if (map->arena) {
        entries[new_idx].key = arena_strdup(map->arena, key);
    } else {
        entries[new_idx].key = strdup(key);
    }
    entries[new_idx].value = (void*)(intptr_t)1;
    
    return 1;
}

void hashmap_free(HashMap *map) {
    // If we used an arena, the arena handles the teardown entirely.
    if (!map || map->arena) return; 
    
    int32_t *indices = (int32_t*)map->buckets;
    DictEntry *entries = (DictEntry*)(indices + map->capacity);
    
    for (int i = 0; i < map->size; i++) {
        // Cast away const specifically for deletion
        free((void*)entries[i].key);
    }
    
    free(map->buckets);
    map->buckets = NULL;
    map->size = 0;
    map->capacity = 0;
}
