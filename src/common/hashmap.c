#include "hashmap.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define HASHMAP_INIT_SIZE 8
#define PERTURB_SHIFT 5
#define TOMBSTONE ((uint32_t)0xFFFFFFFF)

typedef struct {
    uint32_t hash;
    const char *key;
    void *value;
} DictEntry;

static uint32_t murmur3_32(const char *key, size_t len) {
    uint32_t h = 0x811c9dc5;
    const uint8_t *data = (const uint8_t *)key;
    size_t i = 0;

    for (; i + 4 <= len; i += 4) {
        uint32_t k = (uint32_t)data[i]
                   | ((uint32_t)data[i + 1] << 8)
                   | ((uint32_t)data[i + 2] << 16)
                   | ((uint32_t)data[i + 3] << 24);
        k *= 0xcc9e2d51u;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593u;
        h ^= k;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64u;
    }

    uint32_t k = 0;
    switch (len - i) {
        case 3: k ^= (uint32_t)data[i + 2] << 16;
                __attribute__((fallthrough));
        case 2: k ^= (uint32_t)data[i + 1] << 8;
                __attribute__((fallthrough));
        case 1: k ^= (uint32_t)data[i];
                k *= 0xcc9e2d51u;
                k = (k << 15) | (k >> 17);
                k *= 0x1b873593u;
                h ^= k;
    }

    h ^= (uint32_t)len;
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

static uint32_t hash_string(const char *str) {
    return murmur3_32(str, strlen(str));
}

static uint32_t hash_string_n(const char *str, size_t len) {
    return murmur3_32(str, len);
}

static uint32_t next_pow2(uint32_t n) {
    if (n == 0) return 8;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n < 8 ? 8 : n;
}

static void hashmap_resize(HashMap *map) {
    uint32_t old_cap = map->capacity;
    uint32_t new_cap = old_cap * 2;
    size_t new_limit = (size_t)new_cap * 2 / 3;

    int32_t *old_indices = (int32_t *)map->buckets;
    DictEntry *old_entries = (DictEntry *)(old_indices + old_cap);

    size_t new_bytes = (size_t)new_cap * sizeof(int32_t) + new_limit * sizeof(DictEntry);
    void *new_block;

    if (map->arena) {
        new_block = arena_alloc(map->arena, new_bytes);
    } else {
        new_block = malloc(new_bytes);
    }

    if (!new_block) return;

    int32_t *new_indices = (int32_t *)new_block;
    DictEntry *new_entries = (DictEntry *)(new_indices + new_cap);

    for (uint32_t i = 0; i < new_cap; i++) {
        new_indices[i] = -1;
    }

    if (map->size > 0) {
        memcpy(new_entries, old_entries, (size_t)map->size * sizeof(DictEntry));
    }

    size_t mask = new_cap - 1;
    for (uint32_t i = 0; i < map->size; i++) {
        uint32_t hash = new_entries[i].hash;
        size_t perturb = hash;
        size_t j = hash & mask;

        while (new_indices[j] != -1 && new_indices[j] != (int32_t)TOMBSTONE) {
            j = (j * 5 + 1 + perturb) & mask;
            perturb >>= PERTURB_SHIFT;
        }
        new_indices[j] = (int32_t)i;
    }

    if (!map->arena) {
        free(map->buckets);
    }

    map->capacity = new_cap;
    map->buckets = (MapEntry **)new_block;
}

void hashmap_init(HashMap *map, Arena *arena, uint32_t initial_capacity) {
    if (!map) return;

    uint32_t cap = next_pow2(initial_capacity);

    map->capacity = cap;
    map->size = 0;
    map->arena = arena;
    map->owns_keys = false;

    size_t limit = (size_t)cap * 2 / 3;
    size_t bytes = (size_t)cap * sizeof(int32_t) + limit * sizeof(DictEntry);

    if (arena) {
        map->buckets = (MapEntry **)arena_alloc(arena, bytes);
    } else {
        map->buckets = (MapEntry **)malloc(bytes);
    }

    if (!map->buckets) return;

    int32_t *indices = (int32_t *)map->buckets;
    for (uint32_t i = 0; i < cap; i++) {
        indices[i] = -1;
    }
}

void hashmap_put(HashMap *map, const char *key, void *value) {
    if (!map || !key) return;

    size_t limit = (size_t)map->capacity * 2 / 3;
    if (map->size >= limit) {
        hashmap_resize(map);
    }

    uint32_t hash = hash_string(key);
    size_t mask = map->capacity - 1;
    size_t perturb = hash;
    size_t i = hash & mask;

    int32_t *indices = (int32_t *)map->buckets;
    DictEntry *entries = (DictEntry *)(indices + map->capacity);

    while (indices[i] != -1) {
        int idx = indices[i];
        if ((uint32_t)idx != TOMBSTONE) {
            if (entries[idx].hash == hash &&
                (entries[idx].key == key || strcmp(entries[idx].key, key) == 0)) {
                entries[idx].value = value;
                return;
            }
        }
        i = (i * 5 + 1 + perturb) & mask;
        perturb >>= PERTURB_SHIFT;
    }

    int new_idx = (int)map->size++;
    indices[i] = new_idx;

    entries[new_idx].hash = hash;
    if (map->arena) {
        entries[new_idx].key = arena_strdup(map->arena, key);
    } else if (map->owns_keys) {
        entries[new_idx].key = strdup(key);
    } else {
        entries[new_idx].key = key;
    }
    entries[new_idx].value = value;
}

const char* hashmap_intern(HashMap *map, const char *key) {
    if (!map || !key) return NULL;

    size_t limit = (size_t)map->capacity * 2 / 3;
    if (map->size >= limit) {
        hashmap_resize(map);
    }

    uint32_t hash = hash_string(key);
    size_t mask = map->capacity - 1;
    size_t perturb = hash;
    size_t i = hash & mask;

    int32_t *indices = (int32_t *)map->buckets;
    DictEntry *entries = (DictEntry *)(indices + map->capacity);

    while (indices[i] != -1) {
        int idx = indices[i];
        if ((uint32_t)idx != TOMBSTONE) {
            if (entries[idx].hash == hash &&
                (entries[idx].key == key || strcmp(entries[idx].key, key) == 0)) {
                return entries[idx].key;
            }
        }
        i = (i * 5 + 1 + perturb) & mask;
        perturb >>= PERTURB_SHIFT;
    }

    int new_idx = (int)map->size++;
    indices[i] = new_idx;

    entries[new_idx].hash = hash;
    if (map->arena) {
        entries[new_idx].key = arena_strdup(map->arena, key);
    } else if (map->owns_keys) {
        entries[new_idx].key = strdup(key);
    } else {
        entries[new_idx].key = key;
    }
    entries[new_idx].value = (void *)(intptr_t)entries[new_idx].key;

    return entries[new_idx].key;
}

void* hashmap_get(HashMap *map, const char *key) {
    if (!map || !key) return NULL;

    uint32_t hash = hash_string(key);
    size_t mask = map->capacity - 1;
    size_t perturb = hash;
    size_t i = hash & mask;

    int32_t *indices = (int32_t *)map->buckets;
    DictEntry *entries = (DictEntry *)(indices + map->capacity);

    while (indices[i] != -1) {
        int idx = indices[i];
        if ((uint32_t)idx != TOMBSTONE) {
            if (entries[idx].hash == hash &&
                (entries[idx].key == key || strcmp(entries[idx].key, key) == 0)) {
                return entries[idx].value;
            }
        }
        i = (i * 5 + 1 + perturb) & mask;
        perturb >>= PERTURB_SHIFT;
    }

    return NULL;
}

void* hashmap_get_n(HashMap *map, const char *key, size_t len) {
    if (!map || !key) return NULL;

    uint32_t hash = hash_string_n(key, len);
    size_t mask = map->capacity - 1;
    size_t perturb = hash;
    size_t i = hash & mask;

    int32_t *indices = (int32_t *)map->buckets;
    DictEntry *entries = (DictEntry *)(indices + map->capacity);

    while (indices[i] != -1) {
        int idx = indices[i];
        if ((uint32_t)idx != TOMBSTONE) {
            if (entries[idx].hash == hash) {
                size_t stored_len = strlen(entries[idx].key);
                if (stored_len == len && memcmp(entries[idx].key, key, len) == 0) {
                    return entries[idx].value;
                }
            }
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

    size_t limit = (size_t)map->capacity * 2 / 3;
    if (map->size >= limit) {
        hashmap_resize(map);
    }

    uint32_t hash = hash_string(key);
    size_t mask = map->capacity - 1;
    size_t perturb = hash;
    size_t i = hash & mask;

    int32_t *indices = (int32_t *)map->buckets;
    DictEntry *entries = (DictEntry *)(indices + map->capacity);

    while (indices[i] != -1) {
        int idx = indices[i];
        if ((uint32_t)idx != TOMBSTONE) {
            if (entries[idx].hash == hash &&
                (entries[idx].key == key || strcmp(entries[idx].key, key) == 0)) {
                intptr_t count = (intptr_t)entries[idx].value;
                count++;
                entries[idx].value = (void *)count;
                return (int)count;
            }
        }
        i = (i * 5 + 1 + perturb) & mask;
        perturb >>= PERTURB_SHIFT;
    }

    int new_idx = (int)map->size++;
    indices[i] = new_idx;

    entries[new_idx].hash = hash;
    if (map->arena) {
        entries[new_idx].key = arena_strdup(map->arena, key);
    } else if (map->owns_keys) {
        entries[new_idx].key = strdup(key);
    } else {
        entries[new_idx].key = key;
    }
    entries[new_idx].value = (void *)(intptr_t)1;

    return 1;
}

int hashmap_remove(HashMap *map, const char *key) {
    if (!map || !key) return 0;

    uint32_t hash = hash_string(key);
    size_t mask = map->capacity - 1;
    size_t perturb = hash;
    size_t i = hash & mask;

    int32_t *indices = (int32_t *)map->buckets;
    DictEntry *entries = (DictEntry *)(indices + map->capacity);

    while (indices[i] != -1) {
        int idx = indices[i];
        if ((uint32_t)idx != TOMBSTONE) {
            if (entries[idx].hash == hash &&
                (entries[idx].key == key || strcmp(entries[idx].key, key) == 0)) {
                entries[idx].key = NULL;
                entries[idx].value = NULL;
                entries[idx].hash = TOMBSTONE;
                map->size--;
                return 1;
            }
        }
        i = (i * 5 + 1 + perturb) & mask;
        perturb >>= PERTURB_SHIFT;
    }

    return 0;
}

void hashmap_free(HashMap *map) {
    if (!map || map->arena) return;

    if (map->owns_keys) {
        int32_t *indices = (int32_t *)map->buckets;
        DictEntry *entries = (DictEntry *)(indices + map->capacity);
        for (uint32_t i = 0; i < map->size; i++) {
            if (entries[i].key != NULL) {
                free((void *)entries[i].key);
            }
        }
    }

    free(map->buckets);
    map->buckets = NULL;
    map->size = 0;
    map->capacity = 0;
}