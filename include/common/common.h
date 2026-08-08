#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>
#include "common/arena.h"

// --- String Builder ---

typedef struct {
    char *data;
    int len;
    int cap;
    Arena *arena; // Link to the arena
} StringBuilder;

// Initialize with an optional arena. 
// If arena is provided, allocations use the arena and result pointers are managed by it.
// If arena is NULL, uses malloc/realloc/free (legacy mode).
void sb_init(StringBuilder *sb, Arena *arena);

void sb_append(StringBuilder *sb, const char *str);
void sb_append_n(StringBuilder *sb, const char *str, int n);
void sb_append_c(StringBuilder *sb, char c);
void sb_append_fmt(StringBuilder *sb, const char *fmt, ...);
void sb_printf(StringBuilder *sb, const char *fmt, ...);

// Finalizes the string and returns the pointer.
char* sb_return(StringBuilder *sb);

void sb_free(StringBuilder *sb);

void sb_append_escaped(StringBuilder *sb, const char *str);
char* escape_string(const char *str);

// --- Utils ---
char* read_file(const char* path);
void write_file(const char* path, const char* content);
char* read_zip_file(const char *path);

#include <string.h>
static inline int streq_lit(const char *interned, const char *lit) {
    if (interned == lit) return 1;
    if (!interned || !lit) return 0;
    if (interned[0] != lit[0]) return 0;
    return strcmp(interned, lit) == 0;
}

static inline int streq(const char *a, const char *b) {
    return a == b;
}

#include "arena.h"
#include "context.h"
#include "debug.h"
#include "diagnostic.h"
#include "hashmap.h"

#endif // COMMON_H
