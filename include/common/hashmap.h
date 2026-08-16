#ifndef HASHMAP_H
#define HASHMAP_H

#include "arena.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief A hash map entry linking a key to a value.
 */
typedef struct MapEntry {
    char *key;
    void *value;
    struct MapEntry *next;
} MapEntry;

/**
 * @brief A hash map with chaining collision resolution.
 */
typedef struct HashMap {
    MapEntry **buckets;
    uint32_t capacity;
    uint32_t size;
    Arena *arena;
    bool owns_keys;
} HashMap;

/**
 * @brief Initializes a hash map.
 * @param map The hash map to initialize.
 * @param arena The arena allocator, or NULL for malloc.
 * @param initial_capacity The initial bucket count hint.
 */
void hashmap_init(HashMap *map, Arena *arena, uint32_t initial_capacity);
/**
 * @brief Inserts or updates a key-value pair in the hash map.
 * @param map The hash map.
 * @param key The key string.
 * @param value The value to associate with the key.
 */
void hashmap_put(HashMap *map, const char *key, void *value);
/**
 * @brief Retrieves the value associated with a key.
 * @param map The hash map.
 * @param key The key to look up.
 * @return The associated value, or NULL if not found.
 */
void* hashmap_get(HashMap *map, const char *key);
/**
 * @brief Retrieves the value associated with a key of known length.
 * @param map The hash map.
 * @param key The key buffer.
 * @param len The length of the key.
 * @return The associated value, or NULL if not found.
 */
void* hashmap_get_n(HashMap *map, const char *key, size_t len);
/**
 * @brief Checks whether a key exists in the hash map.
 * @param map The hash map.
 * @param key The key to check.
 * @return 1 if the key exists, 0 otherwise.
 */
int hashmap_has(HashMap *map, const char *key);
/**
 * @brief Interns a key string and returns the canonical pointer.
 * @param map The hash map.
 * @param key The key string to intern.
 * @return The canonical pointer for the key, or NULL on failure.
 */
const char* hashmap_intern(HashMap *map, const char *key);

/**
 * @brief Increments the integer counter stored for a key, initializing to 1 if absent.
 * @param map The hash map.
 * @param key The key to increment.
 * @return The new counter value.
 */
int hashmap_inc(HashMap *map, const char *key);

// Removes a key from the map. Returns 1 if the key was found and removed, 0 otherwise.
// Note: uses tombstone markers; does not shrink the map.
/**
 * @brief Removes a key from the hash map.
 * @param map The hash map.
 * @param key The key to remove.
 * @return 1 if the key was removed, 0 if it was not found.
 */
int hashmap_remove(HashMap *map, const char *key);

/**
 * @brief Frees all memory associated with the hash map.
 * @param map The hash map. Does nothing if the map uses an arena.
 */
void hashmap_free(HashMap *map);

#endif // HASHMAP_H
