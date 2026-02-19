#ifndef HASHMAP_H
#define HASHMAP_H

#include "arena.h"
#include <stddef.h>
#include "common.h"

typedef struct MapEntry {
    char *key;
    void *value;
    struct MapEntry *next;
} MapEntry;

typedef struct HashMap {
    MapEntry **buckets;
    int capacity;
    int size;
    Arena *arena;
} HashMap;

void hashmap_init(HashMap *map, Arena *arena, int initial_capacity);
void hashmap_put(HashMap *map, const char *key, void *value);
void* hashmap_get(HashMap *map, const char *key);
int hashmap_has(HashMap *map, const char *key);

// Increments a counter for the given string key and returns the new count.
// Very useful for deduplicating strings (like IR labels).
int hashmap_inc(HashMap *map, const char *key);

// Only needed if not using an Arena allocator
void hashmap_free(HashMap *map);

#endif // HASHMAP_H
