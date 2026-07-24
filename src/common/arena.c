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
}

char* arena_strdup(Arena *a, const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *new_str = (char*)arena_alloc(a, len + 1);
    if (new_str) {
        strcpy(new_str, str);
    }
    return new_str;
}

char* arena_strndup(Arena *a, const char *str, size_t len) {
    if (!str) return NULL;
    char *new_str = (char*)arena_alloc(a, len + 1);
    if (new_str) {
        strncpy(new_str, str, len);
        new_str[len] = '\0';
    }
    return new_str;
}
