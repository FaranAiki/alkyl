#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>
#include "common/arena.h"

// --- Terminal Colors ---

#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"
#define COLOR_UNDERLINE "\033[4m"
#define COLOR_BLINK   "\033[5m"
#define COLOR_REVERSE "\033[7m"
#define COLOR_HIDDEN  "\033[8m"

#define COLOR_BLACK   "\033[30m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

#define COLOR_GRAY    "\033[90m"
#define COLOR_BOLD_RED     "\033[1;31m"
#define COLOR_BOLD_GREEN   "\033[1;32m"
#define COLOR_BOLD_YELLOW  "\033[1;33m"
#define COLOR_BOLD_BLUE    "\033[1;34m"
#define COLOR_BOLD_MAGENTA "\033[1;35m"
#define COLOR_BOLD_CYAN    "\033[1;36m"
#define COLOR_BOLD_WHITE   "\033[1;37m"

// --- Terminal Control ---

#define TERM_CURSOR_UP(n)    "\033[" #n "A"
#define TERM_CURSOR_DOWN(n)  "\033[" #n "B"
#define TERM_CURSOR_FORWARD(n) "\033[" #n "C"
#define TERM_CURSOR_NEXT_LINE "\033[E"
#define TERM_CURSOR_PREV_LINE "\033[F"
#define TERM_CURSOR_COLUMN(n) "\033[" #n "G"

#define CLEAR_LINE      "\033[K"
#define CLEAR_SCREEN    "\033[J"
#define CLEAR_TO_EOL    "\033[K"
#define CLEAR_TO_EOS    "\033[J"

#define CURSOR_HOME     "\033[H"
#define CURSOR_SAVE     "\033[s"
#define CURSOR_RESTORE  "\033[u"

#define PROMPT_COLOR    COLOR_GREEN
#define PROMPT_RESET    COLOR_RESET

// --- String Builder ---

/**
 * @brief A dynamic string builder backed by an arena or malloc.
 */
typedef struct {
    char *data;
    int len;
    int cap;
    Arena *arena; // Link to the arena
} StringBuilder;

/**
 * @brief Initializes a string builder.
 * @param sb The string builder to initialize.
 * @param arena The arena allocator, or NULL for malloc mode.
 */
void sb_init(StringBuilder *sb, Arena *arena);

/**
 * @brief Appends a null-terminated string to the builder.
 * @param sb The string builder.
 * @param str The string to append.
 */
void sb_append(StringBuilder *sb, const char *str);
/**
 * @brief Appends a fixed-length buffer to the builder.
 * @param sb The string builder.
 * @param str The buffer to append.
 * @param n The number of bytes to append.
 */
void sb_append_n(StringBuilder *sb, const char *str, int n);
/**
 * @brief Appends a single character to the builder.
 * @param sb The string builder.
 * @param c The character to append.
 */
void sb_append_c(StringBuilder *sb, char c);
/**
 * @brief Appends a formatted string to the builder.
 * @param sb The string builder.
 * @param fmt The printf-style format string.
 * @param ... Format arguments.
 */
void sb_append_fmt(StringBuilder *sb, const char *fmt, ...);
/**
 * @brief Appends a formatted string to the builder (alias for sb_append_fmt).
 * @param sb The string builder.
 * @param fmt The printf-style format string.
 * @param ... Format arguments.
 */
void sb_printf(StringBuilder *sb, const char *fmt, ...);

/**
 * @brief Finalizes the builder and returns the underlying string.
 * @param sb The string builder.
 * @return The built string (owned by the builder or arena).
 */
char* sb_return(StringBuilder *sb);

/**
 * @brief Frees the string builder's internal buffer (if not arena-backed).
 * @param sb The string builder.
 */
void sb_free(StringBuilder *sb);

/**
 * @brief Appends a string with C-style escaping.
 * @param sb The string builder.
 * @param str The string to append with escapes.
 */
void sb_append_escaped(StringBuilder *sb, const char *str);
/**
 * @brief Returns a newly allocated escaped copy of the input string.
 * @param str The input string.
 * @return A malloc'd escaped string, or NULL on failure.
 */
char* escape_string(const char *str);

/**
 * @brief Reads an entire file into a malloc'd buffer.
 * @param path The path to the file.
 * @return A null-terminated buffer with the file contents, or NULL on failure.
 */
char* read_file(const char* path);
/**
 * @brief Writes a null-terminated string to a file.
 * @param path The path to the output file.
 * @param content The string to write.
 */
void write_file(const char* path, const char* content);
/**
 * @brief Reads the first Alkyl source file from a ZIP archive.
 * @param path The path to the ZIP file.
 * @return A newly allocated buffer with the file contents, or NULL on failure.
 */
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
