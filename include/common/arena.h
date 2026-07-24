#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

// Forward declaration
typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t capacity;
    size_t used;
} ArenaBlock;

typedef struct {
    ArenaBlock *head;
    ArenaBlock *current;
    size_t default_block_size;
} Arena;

// Initialize the arena
void arena_init(Arena *a);

// Allocate memory from the arena. Returns NULL on failure.
// Returned pointer is aligned to sizeof(void*).
void* arena_alloc_slow(Arena *a, size_t aligned_size);

static inline void* arena_alloc(Arena *a, size_t size) {
    if (!a || size == 0) return NULL;
    size_t aligned_size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
    
    if (a->current && a->current->used + aligned_size <= a->current->capacity) {
        uintptr_t addr = (uintptr_t)a->current + sizeof(ArenaBlock) + a->current->used;
        void *ptr = (void *)addr;
        a->current->used += aligned_size;
        return ptr;
    }
    
    void *ptr = arena_alloc_slow(a, aligned_size);
    if (!ptr) {
        // Abort on OOM to prevent LTO warnings about possible NULL pointer dereferences
        extern void abort(void);
        abort();
    }
    return ptr;
}

// Reset the arena for reuse without freeing the allocated blocks.
// Sets the current pointer back to the head and resets usage counters.
void arena_reset(Arena *a);

// Free all memory associated with the arena.
void arena_free(Arena *a);

char* arena_strdup(Arena *a, const char *str);
char* arena_strndup(Arena *a, const char *str, size_t len);

// Helper: Allocate a specific struct/type
#define arena_alloc_type(a, T) ((T*)arena_alloc(a, sizeof(T)))

#endif // ARENA_H
