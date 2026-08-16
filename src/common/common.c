#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * @brief Initializes a string builder.
 * @param sb The string builder to initialize.
 * @param arena The arena allocator, or NULL for malloc mode.
 */
void sb_init(StringBuilder *sb, Arena *arena) {
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    sb->arena = arena;
}

/**
 * @brief Grows the string builder buffer to at least min_cap bytes.
 * @param sb The string builder.
 * @param min_cap The minimum required capacity.
 */
static void sb_grow(StringBuilder *sb, int min_cap) {
    if (sb->cap >= min_cap) return;

    int new_cap = sb->cap == 0 ? 64 : sb->cap * 2;
    if (new_cap < min_cap) new_cap = min_cap;

    if (sb->arena) {
        char *new_data = arena_alloc(sb->arena, new_cap);
        if (sb->data) {
            memcpy(new_data, sb->data, sb->len);
        }
        sb->data = new_data;
    } else {
        char *tmp = realloc(sb->data, new_cap);
        if (!tmp) return;
        sb->data = tmp;
    }
    sb->cap = new_cap;
}

/**
 * @brief Appends a null-terminated string to the builder.
 * @param sb The string builder.
 * @param str The string to append.
 */
void sb_append(StringBuilder *sb, const char *str) {
    if (!str) return;
    int len = (int)strlen(str);
    sb_grow(sb, sb->len + len + 1);
    memcpy(sb->data + sb->len, str, len);
    sb->len += len;
    sb->data[sb->len] = '\0';
}

/**
 * @brief Appends a fixed-length buffer to the builder.
 * @param sb The string builder.
 * @param str The buffer to append.
 * @param n The number of bytes to append.
 */
void sb_append_n(StringBuilder *sb, const char *str, int n) {
    if (!str || n <= 0) return;
    sb_grow(sb, sb->len + n + 1);
    memcpy(sb->data + sb->len, str, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

/**
 * @brief Appends a single character to the builder.
 * @param sb The string builder.
 * @param c The character to append.
 */
void sb_append_c(StringBuilder *sb, char c) {
    sb_grow(sb, sb->len + 2);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

/**
 * @brief Appends a formatted string to the builder.
 * @param sb The string builder.
 * @param fmt The printf-style format string.
 * @param ... Format arguments.
 */
void sb_append_fmt(StringBuilder *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Determine required size
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (len < 0) {
        va_end(args);
        return;
    }

    sb_grow(sb, sb->len + len + 1);
    vsnprintf(sb->data + sb->len, len + 1, fmt, args);
    sb->len += len;

    va_end(args);
}

/**
 * @brief Appends a string with C-style escaping.
 * @param sb The string builder.
 * @param str The string to append with escapes.
 */
void sb_append_escaped(StringBuilder *sb, const char *str) {
    if (!str) return;
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '\n': sb_append(sb, "\\n"); break;
            case '\t': sb_append(sb, "\\t"); break;
            case '\r': sb_append(sb, "\\r"); break;
            case '\\': sb_append(sb, "\\\\"); break;
            case '\"': sb_append(sb, "\\\""); break;
            default:
                sb_append_c(sb, *p);
                break;
        }
    }
}

// TODO: make sure we use arena allocator
/**
 * @brief Returns a newly allocated escaped copy of the input string.
 * @param str The input string.
 * @return A malloc'd escaped string, or NULL on failure.
 */
char* escape_string(const char *str) {
    StringBuilder sb;
    sb_init(&sb, NULL);
    sb_append_escaped(&sb, str);
    return sb_return(&sb);
}

/**
 * @brief Finalizes the builder and returns the underlying string.
 * @param sb The string builder.
 * @return The built string (owned by the builder or arena).
 */
char* sb_return(StringBuilder *sb) {
    if (!sb->data) {
        // Return empty string
        if (sb->arena) return arena_alloc(sb->arena, 1);
        return calloc(1, 1);
    }
    return sb->data;
}

/**
 * @brief Frees the string builder's internal buffer (if not arena-backed).
 * @param sb The string builder.
 */
void sb_free(StringBuilder *sb) {
    if (!sb->arena && sb->data) {
        free(sb->data);
    }
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

/**
 * @brief Reads an entire file into a malloc'd buffer.
 * @param path The path to the file.
 * @return A null-terminated buffer with the file contents, or NULL on failure.
 */
char* read_file(const char* path) {
    size_t len = strlen(path);
    if (len > 4 && streq_lit(path + len - 4, ".zyl")) {
#ifdef HAVE_LIBZIP
        return read_zip_file(path);
#else
        fprintf(stderr, "Cannot unzip '%s': Alkyl is not linked to libzip.\n", path);
        return NULL;
#endif
    }
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(flen + 1);
    if (buf) {
        if (fread(buf, 1, flen, f) != (size_t)flen) {
            free(buf);
            buf = NULL;
        } else {
            buf[flen] = 0;
        }
    }
    fclose(f);
    return buf;
}

/**
 * @brief Writes a null-terminated string to a file.
 * @param path The path to the output file.
 * @param content The string to write.
 */
void write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", content);
        fclose(f);
    }
}
