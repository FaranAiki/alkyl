#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

// Forward declaration
/**
 * @brief A block of memory within an arena allocator.
 */
typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t capacity;
    size_t used;
} ArenaBlock;

/**
 * @brief Interned string storage within an arena.
 */
typedef struct {
    const char **strings;
    size_t capacity;
    size_t count;
} ArenaInterner;

/**
 * @brief The main arena allocator structure.
 */
typedef struct {
    ArenaBlock *head;
    ArenaBlock *current;
    size_t default_block_size;
    ArenaInterner interner;
} Arena;

/**
 * @brief Initializes an arena allocator.
 * @param a The arena to initialize.
 */
void arena_init(Arena *a);

/**
 * @brief Slow path for arena allocation; creates a new block when needed.
 * @param a The arena allocator.
 * @param aligned_size The aligned size to allocate.
 * @return A pointer to the allocated memory, or NULL on failure.
 */
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

/**
 * @brief Resets the arena for reuse without freeing the allocated blocks.
 * @param a The arena allocator.
 */
void arena_reset(Arena *a);

/**
 * @brief Frees all memory associated with the arena.
 * @param a The arena allocator.
 */
void arena_free(Arena *a);

/**
 * @brief Duplicates a string into the arena allocator (also interns it).
 * @param a The arena allocator.
 * @param str The null-terminated string to duplicate.
 * @return A pointer to the duplicated string, or NULL on failure.
 */
char* arena_strdup(Arena *a, const char *str);
/**
 * @brief Duplicates a string of given length into the arena allocator.
 * @param a The arena allocator.
 * @param str The string buffer to duplicate.
 * @param len The length of the string.
 * @return A pointer to the duplicated string, or NULL on failure.
 */
char* arena_strndup(Arena *a, const char *str, size_t len);

// Helper: Allocate a specific struct/type
#define arena_alloc_type(a, T) ((T*)arena_alloc(a, sizeof(T)))

#endif // ARENA_H
