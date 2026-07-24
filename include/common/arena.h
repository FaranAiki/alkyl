#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

// Forward declaration
typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t capacity;
    size_t used;
    // C99 Flexible array member for the actual memory
    char data[]; 
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
        void *ptr = a->current->data + a->current->used;
        a->current->used += aligned_size;
        return ptr;
    }
    
    return arena_alloc_slow(a, aligned_size);
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
