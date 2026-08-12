#include "c_lexer.h"
#include "../common/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>

#define peek(l) ((l)->src[(l)->pos])
static inline char advance(CLexer *l) {
    return l->src[l->pos++];
}

static char* intern_string(CLexer *l, const char *str) {
    return arena_strdup(l->ctx->arena, str);
}

static void c_file_stack_push(CLexer *l, const char *filename, const char *src) {
    if (l->file_stack.count >= l->file_stack.capacity) {
        int new_cap = l->file_stack.capacity == 0 ? 8 : l->file_stack.capacity * 2;
        l->file_stack.entries = arena_alloc(l->ctx->arena, sizeof(CFileStackEntry) * new_cap);
        l->file_stack.capacity = new_cap;
    }
    CFileStackEntry *entry = &l->file_stack.entries[l->file_stack.count++];
    entry->filename = l->filename;
    entry->src = l->src;
    entry->pos = l->pos;
    entry->line = l->line;
    entry->col = l->col;
    l->filename = filename;
    l->src = src;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
}

static int c_file_stack_pop(CLexer *l) {
    if (l->file_stack.count <= 0) return 0;
    l->file_stack.count--;
    CFileStackEntry *entry = &l->file_stack.entries[l->file_stack.count];
    l->filename = entry->filename;
    l->src = entry->src;
    l->pos = entry->pos;
    l->line = entry->line;
    l->col = entry->col;
    l->include_depth--;
    return 1;
}

static char* c_read_include_file(CLexer *l, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = arena_alloc(l->ctx->arena, (size_t)size + 1);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    buf[size] = '\0';
    return buf;
}

void c_lexer_init(CLexer *l, CompilerContext *ctx, const char *filename, const char *src) {
    l->ctx = ctx;
    l->filename = filename;
    l->src = src;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
    l->has_error = 0;
    l->file_stack.entries = NULL;
    l->file_stack.count = 0;
    l->file_stack.capacity = 0;
    l->file_stack.arena = ctx->arena;
    l->included_files = &ctx->import_cache;
    l->include_depth = 0;
}

static void skip_whitespace(CLexer *l) {
    while (1) {
        char c = peek(l);
        if (c == '\0') break;

        if (c == '\\' && l->src[l->pos + 1] == '\n') {
            l->pos += 2;
            l->line++;
            l->col = 1;
            continue;
        }

        if (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            if (c == '\n') { l->line++; l->col = 1; }
            else { l->col++; }
            l->pos++;
            continue;
        }

        if (isspace((unsigned char)c)) {
            advance(l);
            continue;
        }

        break;
    }
}

static void skip_comment(CLexer *l) {
    if (peek(l) == '/' && l->src[l->pos + 1] == '/') {
        l->pos += 2;
        while (peek(l) != '\0' && peek(l) != '\n') {
            l->pos++;
        }
    } else if (peek(l) == '/' && l->src[l->pos + 1] == '*') {
        l->pos += 2;
        while (1) {
            char next = peek(l);
            if (next == '\0') {
                fprintf(stderr, "%s:%d:%d: error: Unclosed block comment\n", l->filename, l->line, l->col);
                l->has_error = 1;
                return;
            }
            if (next == '*' && l->src[l->pos + 1] == '/') {
                l->pos += 2;
                break;
            }
            if (next == '\n') { l->line++; l->col = 1; l->pos++; }
            else { l->pos++; l->col++; }
        }
    }
}

static void skip_to_line_end(CLexer *l) {
    while (peek(l) != '\0') {
        if (peek(l) == '\\' && (l->src[l->pos + 1] == '\n' || (l->src[l->pos + 1] == '\r' && l->src[l->pos + 2] == '\n'))) {
            if (l->src[l->pos + 1] == '\r') l->pos += 3;
            else l->pos += 2;
            l->line++;
            l->col = 1;
            continue;
        }
        if (peek(l) == '\n') {
            break;
        }
        l->pos++;
        l->col++;
    }
}

static CToken c_lex_identifier(CLexer *l, char first);

static HashMap keyword_map;
static int keyword_map_initialized = 0;

static void init_keyword_map(Arena *arena) {
    if (keyword_map_initialized) return;
    hashmap_init(&keyword_map, arena, 256);
    hashmap_put(&keyword_map, "auto", (void*)(intptr_t)C_TOKEN_AUTO);
    hashmap_put(&keyword_map, "break", (void*)(intptr_t)C_TOKEN_BREAK);
    hashmap_put(&keyword_map, "case", (void*)(intptr_t)C_TOKEN_CASE);
    hashmap_put(&keyword_map, "char", (void*)(intptr_t)C_TOKEN_CHAR_KW);
    hashmap_put(&keyword_map, "const", (void*)(intptr_t)C_TOKEN_CONST);
    hashmap_put(&keyword_map, "continue", (void*)(intptr_t)C_TOKEN_CONTINUE);
    hashmap_put(&keyword_map, "default", (void*)(intptr_t)C_TOKEN_DEFAULT);
    hashmap_put(&keyword_map, "do", (void*)(intptr_t)C_TOKEN_DO);
    hashmap_put(&keyword_map, "double", (void*)(intptr_t)C_TOKEN_DOUBLE);
    hashmap_put(&keyword_map, "else", (void*)(intptr_t)C_TOKEN_ELSE);
    hashmap_put(&keyword_map, "enum", (void*)(intptr_t)C_TOKEN_ENUM);
    hashmap_put(&keyword_map, "extern", (void*)(intptr_t)C_TOKEN_EXTERN);
    hashmap_put(&keyword_map, "float", (void*)(intptr_t)C_TOKEN_FLOAT);
    hashmap_put(&keyword_map, "for", (void*)(intptr_t)C_TOKEN_FOR);
    hashmap_put(&keyword_map, "goto", (void*)(intptr_t)C_TOKEN_GOTO);
    hashmap_put(&keyword_map, "if", (void*)(intptr_t)C_TOKEN_IF_KW);
    hashmap_put(&keyword_map, "inline", (void*)(intptr_t)C_TOKEN_INLINE);
    hashmap_put(&keyword_map, "int", (void*)(intptr_t)C_TOKEN_INT);
    hashmap_put(&keyword_map, "long", (void*)(intptr_t)C_TOKEN_LONG);
    hashmap_put(&keyword_map, "register", (void*)(intptr_t)C_TOKEN_REGISTER);
    hashmap_put(&keyword_map, "restrict", (void*)(intptr_t)C_TOKEN_RESTRICT);
    hashmap_put(&keyword_map, "return", (void*)(intptr_t)C_TOKEN_RETURN);
    hashmap_put(&keyword_map, "short", (void*)(intptr_t)C_TOKEN_SHORT);
    hashmap_put(&keyword_map, "signed", (void*)(intptr_t)C_TOKEN_SIGNED);
    hashmap_put(&keyword_map, "sizeof", (void*)(intptr_t)C_TOKEN_SIZEOF_KW);
    hashmap_put(&keyword_map, "static", (void*)(intptr_t)C_TOKEN_STATIC);
    hashmap_put(&keyword_map, "struct", (void*)(intptr_t)C_TOKEN_STRUCT);
    hashmap_put(&keyword_map, "switch", (void*)(intptr_t)C_TOKEN_SWITCH);
    hashmap_put(&keyword_map, "typedef", (void*)(intptr_t)C_TOKEN_TYPEDEF);
    hashmap_put(&keyword_map, "union", (void*)(intptr_t)C_TOKEN_UNION);
    hashmap_put(&keyword_map, "unsigned", (void*)(intptr_t)C_TOKEN_UNSIGNED);
    hashmap_put(&keyword_map, "void", (void*)(intptr_t)C_TOKEN_VOID);
    hashmap_put(&keyword_map, "volatile", (void*)(intptr_t)C_TOKEN_VOLATILE);
    hashmap_put(&keyword_map, "while", (void*)(intptr_t)C_TOKEN_WHILE);
    hashmap_put(&keyword_map, "_Bool", (void*)(intptr_t)C_TOKEN_BOOL_KA);
    hashmap_put(&keyword_map, "bool", (void*)(intptr_t)C_TOKEN_BOOL);
    hashmap_put(&keyword_map, "__stdcall", (void*)(intptr_t)C_TOKEN_STDCALL);
    hashmap_put(&keyword_map, "__cdecl", (void*)(intptr_t)C_TOKEN_CDECL);
    hashmap_put(&keyword_map, "__fastcall", (void*)(intptr_t)C_TOKEN_FASTCALL);
    hashmap_put(&keyword_map, "__thiscall", (void*)(intptr_t)C_TOKEN_THISCALL);
    hashmap_put(&keyword_map, "__vectorcall", (void*)(intptr_t)C_TOKEN_VECTORCALL);
    hashmap_put(&keyword_map, "__declspec", (void*)(intptr_t)C_TOKEN_DECLSPEC);
    hashmap_put(&keyword_map, "__attribute__", (void*)(intptr_t)C_TOKEN_ATTRIBUTE);
    hashmap_put(&keyword_map, "__extension__", (void*)(intptr_t)C_TOKEN_EXTENSION);
    hashmap_put(&keyword_map, "__asm", (void*)(intptr_t)C_TOKEN_ASM);
    hashmap_put(&keyword_map, "asm", (void*)(intptr_t)C_TOKEN_ASM);
    hashmap_put(&keyword_map, "_Complex", (void*)(intptr_t)C_TOKEN_COMPLEX);
    hashmap_put(&keyword_map, "complex", (void*)(intptr_t)C_TOKEN_COMPLEX);
    hashmap_put(&keyword_map, "_Imaginary", (void*)(intptr_t)C_TOKEN_IMAGINARY);
    hashmap_put(&keyword_map, "imaginary", (void*)(intptr_t)C_TOKEN_IMAGINARY);
    hashmap_put(&keyword_map, "true", (void*)(intptr_t)C_TOKEN_TRUE);
    hashmap_put(&keyword_map, "false", (void*)(intptr_t)C_TOKEN_FALSE);
    hashmap_put(&keyword_map, "NULL", (void*)(intptr_t)C_TOKEN_NULL);
    hashmap_put(&keyword_map, "nullptr", (void*)(intptr_t)C_TOKEN_NULLPTR);
    keyword_map_initialized = 1;
}

static CTokenType c_lex_keyword(const char *str) {
    void *val = hashmap_get(&keyword_map, str);
    if (val) {
        return (CTokenType)(intptr_t)val;
    }
    return C_TOKEN_IDENTIFIER;
}

static CToken c_lex_identifier(CLexer *l, char first) {
    CToken t;
    t.line = l->line;
    t.col = l->col;

    int len = 1;
    while (isalnum((unsigned char)peek(l)) || peek(l) == '_') {
        l->pos++;
        l->col++;
        len++;
    }

    char *buf = arena_alloc(l->ctx->arena, len + 1);
    buf[0] = first;
    memcpy(buf + 1, l->src + l->pos - len + 1, len - 1);
    buf[len] = '\0';

    if (!keyword_map_initialized) init_keyword_map(l->ctx->arena);
    t.type = c_lex_keyword(buf);
    t.text = intern_string(l, buf);
    t.int_val = 0;
    t.double_val = 0;
    return t;
}

static CToken c_lex_number(CLexer *l, char first) {
    CToken t;
    t.line = l->line;
    t.col = l->col;
    t.type = C_TOKEN_NUMBER;
    t.text = NULL;
    t.int_val = 0;
    t.double_val = 0;

    long long val = (first >= '0' && first <= '9') ? (first - '0') : 0;

    if (first == '0' && (l->src[l->pos] == 'x' || l->src[l->pos] == 'X')) {
        l->pos++;
        l->col++;
        while (isxdigit((unsigned char)peek(l))) {
            char c = advance(l);
            l->col++;
            int digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else digit = c - 'A' + 10;
            val = val * 16 + digit;
        }
    } else if (first == '0' && (l->src[l->pos] == 'b' || l->src[l->pos] == 'B')) {
        l->pos++;
        l->col++;
        while (peek(l) == '0' || peek(l) == '1') {
            val = val * 2 + (advance(l) - '0');
            l->col++;
        }
    } else {
        while (isdigit((unsigned char)peek(l))) {
            val = val * 10 + (advance(l) - '0');
            l->col++;
        }
    }

    if (peek(l) == 'u' || peek(l) == 'U') { advance(l); l->col++; }
    if (peek(l) == 'l' || peek(l) == 'L') {
        advance(l); l->col++;
        if (peek(l) == 'l' || peek(l) == 'L') { advance(l); l->col++; }
    }
    if (peek(l) == 'f' || peek(l) == 'F') { advance(l); l->col++; }

    t.int_val = val;
    return t;
}

static CToken c_lex_string(CLexer *l) {
    CToken t;
    t.line = l->line;
    t.col = l->col;
    t.type = C_TOKEN_STRING;
    t.int_val = 0;
    t.double_val = 0;

    advance(l); // consume opening "
    int len = 0;
    int cap = 64;
    char *buf = arena_alloc(l->ctx->arena, cap);

    while (peek(l) != '\0' && peek(l) != '"') {
        char c = advance(l);
        l->col++;
        if (c == '\\') {
            char next = peek(l);
            if (next == '\0') break;
            advance(l);
            l->col++;
            switch (next) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case '\'': c = '\''; break;
                case '0': c = '\0'; break;
                case 'a': c = '\a'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'v': c = '\v'; break;
                default: c = next; break;
            }
        }
        if (len + 1 >= cap) {
            cap *= 2;
            buf = arena_alloc(l->ctx->arena, cap);
        }
        buf[len++] = c;
    }

    if (peek(l) == '"') advance(l);

    buf[len] = '\0';
    t.text = intern_string(l, buf);
    return t;
}

static CToken c_lex_char(CLexer *l) {
    CToken t;
    t.line = l->line;
    t.col = l->col;
    t.type = C_TOKEN_CHAR;
    t.text = NULL;
    t.int_val = 0;
    t.double_val = 0;

    advance(l); // consume opening '
    char c = 0;
    if (peek(l) == '\\') {
        advance(l);
        char next = peek(l);
        if (next != '\0') advance(l);
        switch (next) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            case '"': c = '"'; break;
            case '0': c = '\0'; break;
            default: c = next; break;
        }
    } else if (peek(l) != '\0' && peek(l) != '\'') {
        c = advance(l);
    }

    if (peek(l) == '\'') advance(l);

    t.int_val = (unsigned char)c;
    return t;
}

CToken c_lexer_next(CLexer *l) {
    if (l->has_error) {
        CToken eof = {C_TOKEN_EOF, NULL, 0, 0.0, l->line, l->col};
        return eof;
    }

    while (1) {
        skip_whitespace(l);
        if (l->has_error) {
            CToken eof = {C_TOKEN_EOF, NULL, 0, 0.0, l->line, l->col};
            return eof;
        }

        if (peek(l) == '/' && l->src[l->pos + 1] == '/') {
            skip_comment(l);
            continue;
        }
        if (peek(l) == '/' && l->src[l->pos + 1] == '*') {
            skip_comment(l);
            continue;
        }

        if (peek(l) == '#') {
            advance(l);
            skip_whitespace(l);

            int len = 0;
            while (isalpha((unsigned char)peek(l)) || peek(l) == '_') {
                advance(l);
                len++;
            }

            if (len == 0) {
                skip_to_line_end(l);
                continue;
            }

            char *buf = arena_alloc(l->ctx->arena, len + 1);
            memcpy(buf, l->src + l->pos - len, len);
            buf[len] = '\0';

            if (streq_lit(buf, "include")) {
                if (l->include_depth >= 50) {
                    skip_to_line_end(l);
                    continue;
                }
                skip_whitespace(l);
                char include_type = 0;
                if (peek(l) == '"') include_type = '"';
                else if (peek(l) == '<') include_type = '<';
                
                if (include_type) {
                    char end_char = (include_type == '"') ? '"' : '>';
                    advance(l);
                    int fname_len = 0;
                    int start_pos = l->pos;
                    while (peek(l) != '\0' && peek(l) != end_char && peek(l) != '\n') {
                        advance(l);
                        fname_len++;
                    }
                    if (peek(l) == end_char) advance(l);
                    
                    char *fname_buf = arena_alloc(l->ctx->arena, fname_len + 1);
                    memcpy(fname_buf, l->src + start_pos, fname_len);
                    fname_buf[fname_len] = '\0';
                    
                    char *full_path = NULL;
                    if (include_type == '"') {
                        int dir_len = 0;
                        const char *slash = strrchr(l->filename, '/');
                        if (slash) dir_len = (int)(slash - l->filename) + 1;
                        full_path = arena_alloc(l->ctx->arena, dir_len + fname_len + 1);
                        memcpy(full_path, l->filename, dir_len);
                        memcpy(full_path + dir_len, fname_buf, fname_len);
                        full_path[dir_len + fname_len] = '\0';
                    } else {
                        full_path = arena_alloc(l->ctx->arena, fname_len + 12);
                        sprintf(full_path, "/usr/include/%s", fname_buf);
                    }
                    
                    if (hashmap_has(l->included_files, full_path)) {
                        skip_to_line_end(l);
                        continue;
                    }
                    hashmap_put(l->included_files, full_path, (void*)1);
                    
                    char *inc_src = c_read_include_file(l, full_path);
                    if (inc_src) {
                        l->include_depth++;
                        c_file_stack_push(l, full_path, inc_src);
                    } else {
                        skip_to_line_end(l);
                    }
                    continue;
                }
            }

            if (streq_lit(buf, "define") || streq_lit(buf, "ifdef") ||
                streq_lit(buf, "ifndef") || streq_lit(buf, "if") ||
                streq_lit(buf, "else") || streq_lit(buf, "elif") ||
                streq_lit(buf, "endif") || streq_lit(buf, "undef") ||
                streq_lit(buf, "pragma") || streq_lit(buf, "error") ||
                streq_lit(buf, "line")) {
                skip_to_line_end(l);
                continue;
            }

            skip_to_line_end(l);
            continue;
        }

        break;
    }

    CToken t;
    t.line = l->line;
    t.col = l->col;
    t.text = NULL;
    t.int_val = 0;
    t.double_val = 0;

    char c = peek(l);
    if (c == '\0') {
        if (l->file_stack.count > 0) {
            c_file_stack_pop(l);
            return c_lexer_next(l);
        }
        t.type = C_TOKEN_EOF;
        return t;
    }

    if (isalpha((unsigned char)c) || c == '_') {
        return c_lex_identifier(l, advance(l));
    }

    if (isdigit((unsigned char)c)) {
        return c_lex_number(l, advance(l));
    }

    if (c == '"') {
        return c_lex_string(l);
    }

    if (c == '\'') {
        return c_lex_char(l);
    }

    // Two-character operators
    if (c == '-' && l->src[l->pos + 1] == '>') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_ARROW;
        return t;
    }
    if (c == '-' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_MINUS_ASSIGN;
        return t;
    }
    if (c == '+' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_PLUS_ASSIGN;
        return t;
    }
    if (c == '+' && l->src[l->pos + 1] == '+') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_INCREMENT;
        return t;
    }
    if (c == '=' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_EQ;
        return t;
    }
    if (c == '!' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_NEQ;
        return t;
    }
    if (c == '<' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_LTE;
        return t;
    }
    if (c == '>' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_GTE;
        return t;
    }
    if (c == '&' && l->src[l->pos + 1] == '&') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_AND;
        return t;
    }
    if (c == '|' && l->src[l->pos + 1] == '|') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_OR;
        return t;
    }
    if (c == '&' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_AND_ASSIGN;
        return t;
    }
    if (c == '|' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_OR_ASSIGN;
        return t;
    }
    if (c == '^' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_XOR_ASSIGN;
        return t;
    }
    if (c == '<' && l->src[l->pos + 1] == '<') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_LSHIFT;
        return t;
    }
    if (c == '>' && l->src[l->pos + 1] == '>') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_RSHIFT;
        return t;
    }
    if (c == '<' && l->src[l->pos + 1] == '<' && l->src[l->pos + 2] == '=') {
        l->pos += 3; l->col += 3;
        t.type = C_TOKEN_LSHIFT_ASSIGN;
        return t;
    }
    if (c == '>' && l->src[l->pos + 1] == '>' && l->src[l->pos + 2] == '=') {
        l->pos += 3; l->col += 3;
        t.type = C_TOKEN_RSHIFT_ASSIGN;
        return t;
    }
    if (c == '*' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_STAR_ASSIGN;
        return t;
    }
    if (c == '/' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_SLASH_ASSIGN;
        return t;
    }
    if (c == '%' && l->src[l->pos + 1] == '=') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_MOD_ASSIGN;
        return t;
    }
    if (c == '-' && l->src[l->pos + 1] == '-') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_DECREMENT;
        return t;
    }
    if (c == '.' && l->src[l->pos + 1] == '.' && l->src[l->pos + 2] == '.') {
        l->pos += 3; l->col += 3;
        t.type = C_TOKEN_ELLIPSIS;
        return t;
    }
    if (c == '.' && l->src[l->pos + 1] == '*') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_DOT_STAR;
        return t;
    }
    if (c == '-' && l->src[l->pos + 1] == '>') {
        l->pos += 2; l->col += 2;
        t.type = C_TOKEN_ARROW_STAR;
        return t;
    }

    l->pos++; l->col++;
    switch (c) {
        case '(': t.type = C_TOKEN_LPAREN; break;
        case ')': t.type = C_TOKEN_RPAREN; break;
        case '[': t.type = C_TOKEN_LBRACKET; break;
        case ']': t.type = C_TOKEN_RBRACKET; break;
        case '{': t.type = C_TOKEN_LBRACE; break;
        case '}': t.type = C_TOKEN_RBRACE; break;
        case ';': t.type = C_TOKEN_SEMICOLON; break;
        case ',': t.type = C_TOKEN_COMMA; break;
        case '.': t.type = C_TOKEN_DOT; break;
        case '~': t.type = C_TOKEN_TILDE; break;
        case '!': t.type = C_TOKEN_NOT; break;
        case '^': t.type = C_TOKEN_XOR; break;
        case '+': t.type = C_TOKEN_PLUS; break;
        case '-': t.type = C_TOKEN_MINUS; break;
        case '*': t.type = C_TOKEN_STAR; break;
        case '/': t.type = C_TOKEN_SLASH; break;
        case '%': t.type = C_TOKEN_PERCENT; break;
        case '=': t.type = C_TOKEN_ASSIGN; break;
        case '<': t.type = C_TOKEN_LT; break;
        case '>': t.type = C_TOKEN_GT; break;
        case '&': t.type = C_TOKEN_AMPERSAND; break;
        case '|': t.type = C_TOKEN_OR; break;
        case ':': t.type = C_TOKEN_COLON; break;
        case '?': t.type = C_TOKEN_QUESTION; break;
        default:
            fprintf(stderr, "%s:%d:%d: warning: unknown character '%c'\n", l->filename, l->line, l->col, c);
            t.type = C_TOKEN_UNKNOWN;
            break;
    }
    return t;
}

const char* c_token_type_to_string(CTokenType type) {
    switch (type) {
        case C_TOKEN_EOF: return "EOF";
        case C_TOKEN_IDENTIFIER: return "IDENTIFIER";
        case C_TOKEN_NUMBER: return "NUMBER";
        case C_TOKEN_STRING: return "STRING";
        case C_TOKEN_CHAR: return "CHAR";
        case C_TOKEN_LPAREN: return "'('";
        case C_TOKEN_RPAREN: return "')'";
        case C_TOKEN_LBRACKET: return "'['";
        case C_TOKEN_RBRACKET: return "']'";
        case C_TOKEN_LBRACE: return "'{'";
        case C_TOKEN_RBRACE: return "'}'";
        case C_TOKEN_SEMICOLON: return "';'";
        case C_TOKEN_COMMA: return "','";
        case C_TOKEN_STAR: return "'*'";
        case C_TOKEN_AMPERSAND: return "'&'";
        case C_TOKEN_STRUCT: return "'struct'";
        case C_TOKEN_UNION: return "'union'";
        case C_TOKEN_ENUM: return "'enum'";
        case C_TOKEN_TYPEDEF: return "'typedef'";
        case C_TOKEN_EXTERN: return "'extern'";
        default: return "UNKNOWN";
    }
}
