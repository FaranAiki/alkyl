#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Default to 4KB blocks if not specified
#ifndef ARENA_BLOCK_SIZE
#define ARENA_BLOCK_SIZE (4 * 1024 * 16) // 64 KB
#endif

#define ARENA_ALIGNMENT sizeof(void*)

// ArenaBlock is now defined in arena.h

void arena_init(Arena *a) {
    if (a) {
        a->head = NULL;
        a->current = NULL;
        a->default_block_size = ARENA_BLOCK_SIZE;
        a->interner.capacity = 1024;
        a->interner.count = 0;
        a->interner.strings = (const char**)malloc(sizeof(const char*) * 1024);
        if (a->interner.strings) {
            memset(a->interner.strings, 0, sizeof(const char*) * 1024);
        }
    }
}

static ArenaBlock* arena_create_block(size_t size) {
    if (size > SIZE_MAX - sizeof(ArenaBlock)) return NULL;
    ArenaBlock *block = (ArenaBlock*)malloc(sizeof(ArenaBlock) + size);
    if (block) {
        block->next = NULL;
        block->capacity = size;
        block->used = 0;
    }
    return block;
}

void* arena_alloc_slow(Arena *a, size_t aligned_size) {
    if (a->current && a->current->next) {
        ArenaBlock *next = a->current->next;
        if (next->capacity >= aligned_size) {
            a->current = next;
            uintptr_t addr = (uintptr_t)a->current + sizeof(ArenaBlock) + a->current->used;
            void *ptr = (void *)addr;
            a->current->used += aligned_size;
            return ptr;
        }
    }

    size_t block_size = (aligned_size > a->default_block_size) ? aligned_size : a->default_block_size;
    ArenaBlock *new_block = arena_create_block(block_size);
    if (!new_block) return NULL;

    if (!a->head) {
        a->head = new_block;
        a->current = new_block;
    } else {
        new_block->next = a->current->next;
        a->current->next = new_block;
        a->current = new_block;
    }

    uintptr_t addr = (uintptr_t)new_block + sizeof(ArenaBlock) + new_block->used;
    void *ptr = (void *)addr;
    new_block->used += aligned_size;
    return ptr;
}

void arena_reset(Arena *a) {
    if (!a) return;
    ArenaBlock *block = a->head;
    while (block) {
        block->used = 0;
        block = block->next;
    }
    a->current = a->head;
}

void arena_free(Arena *a) {
    if (!a) return;
    ArenaBlock *block = a->head;
    while (block) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    a->head = NULL;
    a->current = NULL;
    if (a->interner.strings) {
        free(a->interner.strings);
        a->interner.strings = NULL;
    }
}

static uint32_t hash_str(const char *str, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)str[i];
        hash *= 16777619;
    }
    return hash;
}

static const char* intern_string(Arena *a, const char *str, size_t len) {
    if (!a || !a->interner.strings) {
        char *new_str = (char*)arena_alloc(a, len + 1);
        if (new_str) {
            memcpy(new_str, str, len);
            new_str[len] = '\0';
        }
        return new_str;
    }

    if (a->interner.count * 2 >= a->interner.capacity) {
        size_t old_cap = a->interner.capacity;
        const char **old_strings = a->interner.strings;
        
        a->interner.capacity = old_cap ? old_cap * 2 : 1024;
        a->interner.strings = (const char**)calloc(a->interner.capacity, sizeof(const char*));
        a->interner.count = 0;
        
        if (old_strings) {
            for (size_t i = 0; i < old_cap; i++) {
                if (old_strings[i]) {
                    uint32_t h = hash_str(old_strings[i], strlen(old_strings[i])) & (a->interner.capacity - 1);
                    while (a->interner.strings[h]) {
                        h = (h + 1) & (a->interner.capacity - 1);
                    }
                    a->interner.strings[h] = old_strings[i];
                    a->interner.count++;
                }
            }
            free(old_strings);
        }
    }

    uint32_t h = hash_str(str, len) & (a->interner.capacity - 1);
    while (a->interner.strings[h]) {
        const char *existing = a->interner.strings[h];
        if (strncmp(existing, str, len) == 0 && existing[len] == '\0') {
            return existing;
        }
        h = (h + 1) & (a->interner.capacity - 1);
    }

    char *new_str = (char*)arena_alloc(a, len + 1);
    if (new_str) {
        memcpy(new_str, str, len);
        new_str[len] = '\0';
        a->interner.strings[h] = new_str;
        a->interner.count++;
    }
    return new_str;
}

char* arena_strdup(Arena *a, const char *str) {
    if (!str) return NULL;
    return (char*)intern_string(a, str, strlen(str));
}

char* arena_strndup(Arena *a, const char *str, size_t len) {
    if (!str) return NULL;
    return (char*)intern_string(a, str, len);
}
