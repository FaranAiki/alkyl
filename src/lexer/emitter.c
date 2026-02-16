#include "emitter.h"
#include "../diagnostic/diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// --- Private String Builder Utility ---

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StringBuilder;

static void sb_init(StringBuilder *sb) {
    sb->cap = 1024;
    sb->len = 0;
    sb->data = malloc(sb->cap);
    if (sb->data) sb->data[0] = '\0';
}

static void sb_append_fmt(StringBuilder *sb, const char *fmt, ...) {
    if (!sb->data) return;
    va_list args;
    va_start(args, fmt);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (len < 0) { va_end(args); return; }

    if (sb->len + len + 1 >= sb->cap) {
        sb->cap = (sb->cap * 2) + len;
        char *new_data = realloc(sb->data, sb->cap);
        if (!new_data) { free(sb->data); sb->data = NULL; va_end(args); return; }
        sb->data = new_data;
    }
    
    vsnprintf(sb->data + sb->len, len + 1, fmt, args);
    sb->len += len;
    va_end(args);
}

static char* sb_free_and_return(StringBuilder *sb) {
    return sb->data; // Ownership transferred to caller
}

char* lexer_to_string(Lexer *l) {
    StringBuilder sb;
    sb_init(&sb);
    if (!sb.data) return NULL;

    Token t = lexer_next(l);
    while (t.type != TOKEN_EOF) {
        const char *type_str = token_type_to_string(t.type);
        
        sb_append_fmt(&sb, "[%-15s] ", type_str);
        
        // Value info
        if (t.text) {
            sb_append_fmt(&sb, "'%s'", t.text);
        } else if (t.type == TOKEN_NUMBER) {
            sb_append_fmt(&sb, "%d", t.int_val);
        } else if (t.type == TOKEN_FLOAT) {
            sb_append_fmt(&sb, "%f", t.double_val);
        } else if (t.type == TOKEN_STRING) {
            sb_append_fmt(&sb, "\"%s\"", t.text ? t.text : "");
        }

        sb_append_fmt(&sb, "\t(Line: %d, Col: %d)\n", t.line, t.col);
        
        if (t.text) free(t.text);
        
        t = lexer_next(l);
    }
    
    return sb_free_and_return(&sb);
}

void lexer_to_file(Lexer *l, const char *filename) {
    char *str = lexer_to_string(l);
    if (str) {
        FILE *f = fopen(filename, "w");
        if (f) {
            fputs(str, f);
            fclose(f);
        }
        free(str);
    }
}

char* lexer_string_to_string(const char *src) {
    Lexer l;
    lexer_init(&l, src);
    return lexer_to_string(&l);
}

void lexer_string_to_file(const char *src, const char *filename) {
    Lexer l;
    lexer_init(&l, src);
    lexer_to_file(&l, filename);
}
