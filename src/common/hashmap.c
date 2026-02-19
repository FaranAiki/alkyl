#include "hashmap.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

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
    int new_capacity = map->capacity * 2;
    MapEntry **new_buckets;
    
    if (map->arena) {
        new_buckets = (MapEntry**)arena_alloc(map->arena, sizeof(MapEntry*) * new_capacity);
        memset(new_buckets, 0, sizeof(MapEntry*) * new_capacity);
    } else {
        new_buckets = (MapEntry**)calloc(new_capacity, sizeof(MapEntry*));
    }

    // Rehash all existing entries into the new buckets
    for (int i = 0; i < map->capacity; i++) {
        MapEntry *entry = map->buckets[i];
        while (entry) {
            MapEntry *next = entry->next;
            uint32_t hash = hash_string(entry->key);
            int index = hash % new_capacity;
            
            entry->next = new_buckets[index];
            new_buckets[index] = entry;
            
            entry = next;
        }
    }

    if (!map->arena) {
        free(map->buckets);
    }
    
    map->buckets = new_buckets;
    map->capacity = new_capacity;
}

void hashmap_init(HashMap *map, Arena *arena, int initial_capacity) {
    if (!map) return;
    if (initial_capacity <= 0) initial_capacity = 64; // Default
    
    map->capacity = initial_capacity;
    map->size = 0;
    map->arena = arena;
    
    size_t buckets_size = sizeof(MapEntry*) * initial_capacity;
    if (arena) {
        map->buckets = (MapEntry**)arena_alloc(arena, buckets_size);
        memset(map->buckets, 0, buckets_size);
    } else {
        map->buckets = (MapEntry**)calloc(initial_capacity, sizeof(MapEntry*));
    }
}

void hashmap_put(HashMap *map, const char *key, void *value) {
    if (!map || !key) return;
    
    // Resize if load factor >= 0.75
    if (map->size * 4 >= map->capacity * 3) {
        hashmap_resize(map);
    }
    
    uint32_t hash = hash_string(key);
    int index = hash % map->capacity;
    
    MapEntry *entry = map->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // Add new entry
    MapEntry *new_entry;
    if (map->arena) {
        new_entry = (MapEntry*)arena_alloc(map->arena, sizeof(MapEntry));
        new_entry->key = arena_strdup(map->arena, key);
    } else {
        new_entry = (MapEntry*)malloc(sizeof(MapEntry));
        new_entry->key = strdup(key);
    }
    
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->size++;
}

void* hashmap_get(HashMap *map, const char *key) {
    if (!map || !key) return NULL;
    
    uint32_t hash = hash_string(key);
    int index = hash % map->capacity;
    
    MapEntry *entry = map->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

int hashmap_has(HashMap *map, const char *key) {
    return hashmap_get(map, key) != NULL;
}

int hashmap_inc(HashMap *map, const char *key) {
    if (!map || !key) return 0;
    
    // Resize if load factor >= 0.75
    if (map->size * 4 >= map->capacity * 3) {
        hashmap_resize(map);
    }
    
    uint32_t hash = hash_string(key);
    int index = hash % map->capacity;
    
    MapEntry *entry = map->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            // We cast the pointer to intptr_t to use it as a counter 
            intptr_t count = (intptr_t)entry->value;
            count++;
            entry->value = (void*)count;
            return (int)count;
        }
        entry = entry->next;
    }
    
    // Add new entry with count 1
    MapEntry *new_entry;
    if (map->arena) {
        new_entry = (MapEntry*)arena_alloc(map->arena, sizeof(MapEntry));
        new_entry->key = arena_strdup(map->arena, key);
    } else {
        new_entry = (MapEntry*)malloc(sizeof(MapEntry));
        new_entry->key = strdup(key);
    }
    
    new_entry->value = (void*)(intptr_t)1;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->size++;
    
    return 1;
}

void hashmap_free(HashMap *map) {
    // If we used an arena, the arena handles the teardown.
    if (!map || map->arena) return; 
    
    for (int i = 0; i < map->capacity; i++) {
        MapEntry *entry = map->buckets[i];
        while (entry) {
            MapEntry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(map->buckets);
    map->buckets = NULL;
    map->size = 0;
}
