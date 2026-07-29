#ifndef HASHMAP_H
#define HASHMAP_H

#include "arena.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct MapEntry {
    char *key;
    void *value;
    struct MapEntry *next;
} MapEntry;

typedef struct HashMap {
    MapEntry **buckets;
    uint32_t capacity;
    uint32_t size;
    Arena *arena;
    bool owns_keys;
} HashMap;

void hashmap_init(HashMap *map, Arena *arena, uint32_t initial_capacity);
void hashmap_put(HashMap *map, const char *key, void *value);
void* hashmap_get(HashMap *map, const char *key);
void* hashmap_get_n(HashMap *map, const char *key, size_t len);
int hashmap_has(HashMap *map, const char *key);
const char* hashmap_intern(HashMap *map, const char *key);

// Increments a counter for the given string key and returns the new count.
// Very useful for deduplicating strings (like IR labels).
int hashmap_inc(HashMap *map, const char *key);

// Removes a key from the map. Returns 1 if the key was found and removed, 0 otherwise.
// Note: uses tombstone markers; does not shrink the map.
int hashmap_remove(HashMap *map, const char *key);

// Only needed if not using an Arena allocator
void hashmap_free(HashMap *map);

#endif // HASHMAP_H
