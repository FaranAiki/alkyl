#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

void sb_init(StringBuilder *sb, Arena *arena) {
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    sb->arena = arena;
}

static void sb_grow(StringBuilder *sb, int min_cap) {
    if (sb->cap >= min_cap) return;
    
    int new_cap = sb->cap == 0 ? 64 : sb->cap * 2;
    if (new_cap < min_cap) new_cap = min_cap;
    
    if (sb->arena) {
        // Arena mode: Allocate new block and copy.
        // This trades arena memory usage for safety/speed (no manual free).
        char *new_data = arena_alloc(sb->arena, new_cap);
        if (sb->data) {
            memcpy(new_data, sb->data, sb->len);
        }
        sb->data = new_data;
    } else {
        // Legacy mode: standard realloc
        sb->data = realloc(sb->data, new_cap);
    }
    sb->cap = new_cap;
}

void sb_append(StringBuilder *sb, const char *str) {
    if (!str) return;
    int len = (int)strlen(str);
    sb_grow(sb, sb->len + len + 1);
    memcpy(sb->data + sb->len, str, len);
    sb->len += len;
    sb->data[sb->len] = '\0';
}

void sb_append_n(StringBuilder *sb, const char *str, int n) {
    if (!str || n <= 0) return;
    sb_grow(sb, sb->len + n + 1);
    memcpy(sb->data + sb->len, str, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void sb_append_c(StringBuilder *sb, char c) {
    sb_grow(sb, sb->len + 2);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

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

char* escape_string(const char *str) {
    StringBuilder sb;
    sb_init(&sb, NULL);
    sb_append_escaped(&sb, str);
    return sb_return(&sb);
}

char* sb_return(StringBuilder *sb) {
    if (!sb->data) {
        // Return empty string
        if (sb->arena) return arena_alloc(sb->arena, 1);
        return calloc(1, 1);
    }
    return sb->data;
}

void sb_free(StringBuilder *sb) {
    if (!sb->arena && sb->data) {
        free(sb->data);
    }
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(len + 1);
    if (buf) {
        if (fread(buf, 1, len, f) != (size_t)len) {
            free(buf);
            buf = NULL;
        } else {
            buf[len] = 0;
        }
    }
    fclose(f);
    return buf;
}

void write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", content);
        fclose(f);
    }
}
