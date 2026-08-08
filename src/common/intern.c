#include "common/intern.h"
#include "common/hashmap.h"
#include "common/arena.h"
#include <stdlib.h>
#include <string.h>

static HashMap g_intern_map;
static Arena g_intern_arena;
static bool g_intern_inited = false;

static void ensure_intern_inited(void) {
    if (!g_intern_inited) {
        arena_init(&g_intern_arena, 64 * 1024);
        hashmap_init(&g_intern_map, &g_intern_arena, 1024);
        g_intern_inited = true;
    }
}

const char* intern_string(const char* str) {
    if (!str) return NULL;
    ensure_intern_inited();

    void *existing = hashmap_get(&g_intern_map, str);
    if (existing) {
        return (const char*)existing;
    }

    char *new_str = arena_strdup(&g_intern_arena, str);
    hashmap_put(&g_intern_map, new_str, new_str);
    return new_str;
}

const char* intern_string_len(const char* str, size_t len) {
    if (!str) return NULL;
    ensure_intern_inited();

    void *existing = hashmap_get_n(&g_intern_map, str, len);
    if (existing) {
        return (const char*)existing;
    }

    char *new_str = (char*)arena_alloc(&g_intern_arena, len + 1);
    memcpy(new_str, str, len);
    new_str[len] = '\0';
    hashmap_put(&g_intern_map, new_str, new_str);
    return new_str;
}
