#include "c_lexer.h"
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

void c_lexer_init(CLexer *l, CompilerContext *ctx, const char *filename, const char *src) {
    l->src = src;
    l->filename = filename;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
    l->ctx = ctx;
    l->has_error = 0;
}

static void skip_whitespace(CLexer *l) {
    while (1) {
        char c = peek(l);
        if (c == '\0') break;

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
    while (peek(l) != '\0' && peek(l) != '\n') {
        l->pos++;
    }
}

static int c_lex_keyword(const char *str) {
    if (strcmp(str, "auto") == 0) return C_TOKEN_AUTO;
    if (strcmp(str, "break") == 0) return C_TOKEN_BREAK;
    if (strcmp(str, "case") == 0) return C_TOKEN_CASE;
    if (strcmp(str, "char") == 0) return C_TOKEN_CHAR_KW;
    if (strcmp(str, "const") == 0) return C_TOKEN_CONST;
    if (strcmp(str, "continue") == 0) return C_TOKEN_CONTINUE;
    if (strcmp(str, "default") == 0) return C_TOKEN_DEFAULT;
    if (strcmp(str, "do") == 0) return C_TOKEN_DO;
    if (strcmp(str, "double") == 0) return C_TOKEN_DOUBLE;
    if (strcmp(str, "else") == 0) return C_TOKEN_ELSE;
    if (strcmp(str, "enum") == 0) return C_TOKEN_ENUM;
    if (strcmp(str, "extern") == 0) return C_TOKEN_EXTERN;
    if (strcmp(str, "float") == 0) return C_TOKEN_FLOAT;
    if (strcmp(str, "for") == 0) return C_TOKEN_FOR;
    if (strcmp(str, "goto") == 0) return C_TOKEN_GOTO;
    if (strcmp(str, "if") == 0) return C_TOKEN_IF_KW;
    if (strcmp(str, "inline") == 0) return C_TOKEN_INLINE;
    if (strcmp(str, "int") == 0) return C_TOKEN_INT;
    if (strcmp(str, "long") == 0) return C_TOKEN_LONG;
    if (strcmp(str, "register") == 0) return C_TOKEN_REGISTER;
    if (strcmp(str, "restrict") == 0) return C_TOKEN_RESTRICT;
    if (strcmp(str, "return") == 0) return C_TOKEN_RETURN;
    if (strcmp(str, "short") == 0) return C_TOKEN_SHORT;
    if (strcmp(str, "signed") == 0) return C_TOKEN_SIGNED;
    if (strcmp(str, "sizeof") == 0) return C_TOKEN_SIZEOF_KW;
    if (strcmp(str, "static") == 0) return C_TOKEN_STATIC;
    if (strcmp(str, "struct") == 0) return C_TOKEN_STRUCT;
    if (strcmp(str, "switch") == 0) return C_TOKEN_SWITCH;
    if (strcmp(str, "typedef") == 0) return C_TOKEN_TYPEDEF;
    if (strcmp(str, "union") == 0) return C_TOKEN_UNION;
    if (strcmp(str, "unsigned") == 0) return C_TOKEN_UNSIGNED;
    if (strcmp(str, "void") == 0) return C_TOKEN_VOID;
    if (strcmp(str, "volatile") == 0) return C_TOKEN_VOLATILE;
    if (strcmp(str, "while") == 0) return C_TOKEN_WHILE;
    if (strcmp(str, "_Bool") == 0 || strcmp(str, "bool") == 0) return C_TOKEN_BOOL;
    // MSVC / Windows
    if (strcmp(str, "__stdcall") == 0) return C_TOKEN_STDCALL;
    if (strcmp(str, "__cdecl") == 0) return C_TOKEN_CDECL;
    if (strcmp(str, "__fastcall") == 0) return C_TOKEN_FASTCALL;
    if (strcmp(str, "__thiscall") == 0) return C_TOKEN_THISCALL;
    if (strcmp(str, "__vectorcall") == 0) return C_TOKEN_VECTORCALL;
    if (strcmp(str, "__declspec") == 0) return C_TOKEN_DECLSPEC;
    if (strcmp(str, "__attribute__") == 0) return C_TOKEN_ATTRIBUTE;
    if (strcmp(str, "__extension__") == 0) return C_TOKEN_EXTENSION;
    if (strcmp(str, "__asm") == 0 || strcmp(str, "asm") == 0) return C_TOKEN_ASM;
    // Standard types
    if (strcmp(str, "size_t") == 0) return C_TOKEN_SIZE_T;
    if (strcmp(str, "ptrdiff_t") == 0) return C_TOKEN_PTRDIFF_T;
    if (strcmp(str, "wchar_t") == 0) return C_TOKEN_WCHAR_T;
    if (strcmp(str, "char16_t") == 0) return C_TOKEN_CHAR16_T;
    if (strcmp(str, "char32_t") == 0) return C_TOKEN_CHAR32_T;
    if (strcmp(str, "int8_t") == 0) return C_TOKEN_INT8_T;
    if (strcmp(str, "int16_t") == 0) return C_TOKEN_INT16_T;
    if (strcmp(str, "int32_t") == 0) return C_TOKEN_INT32_T;
    if (strcmp(str, "int64_t") == 0) return C_TOKEN_INT64_T;
    if (strcmp(str, "uint8_t") == 0) return C_TOKEN_UINT8_T;
    if (strcmp(str, "uint16_t") == 0) return C_TOKEN_UINT16_T;
    if (strcmp(str, "uint32_t") == 0) return C_TOKEN_UINT32_T;
    if (strcmp(str, "uint64_t") == 0) return C_TOKEN_UINT64_T;
    if (strcmp(str, "int_least8_t") == 0) return C_TOKEN_INT_LEAST8_T;
    if (strcmp(str, "int_least16_t") == 0) return C_TOKEN_INT_LEAST16_T;
    if (strcmp(str, "int_least32_t") == 0) return C_TOKEN_INT_LEAST32_T;
    if (strcmp(str, "int_least64_t") == 0) return C_TOKEN_INT_LEAST64_T;
    if (strcmp(str, "uint_least8_t") == 0) return C_TOKEN_UINT_LEAST8_T;
    if (strcmp(str, "uint_least16_t") == 0) return C_TOKEN_UINT_LEAST16_T;
    if (strcmp(str, "uint_least32_t") == 0) return C_TOKEN_UINT_LEAST32_T;
    if (strcmp(str, "uint_least64_t") == 0) return C_TOKEN_UINT_LEAST64_T;
    if (strcmp(str, "int_fast8_t") == 0) return C_TOKEN_INT_FAST8_T;
    if (strcmp(str, "int_fast16_t") == 0) return C_TOKEN_INT_FAST16_T;
    if (strcmp(str, "int_fast32_t") == 0) return C_TOKEN_INT_FAST32_T;
    if (strcmp(str, "int_fast64_t") == 0) return C_TOKEN_INT_FAST64_T;
    if (strcmp(str, "uint_fast8_t") == 0) return C_TOKEN_UINT_FAST8_T;
    if (strcmp(str, "uint_fast16_t") == 0) return C_TOKEN_UINT_FAST16_T;
    if (strcmp(str, "uint_fast32_t") == 0) return C_TOKEN_UINT_FAST32_T;
    if (strcmp(str, "uint_fast64_t") == 0) return C_TOKEN_UINT_FAST64_T;
    if (strcmp(str, "intmax_t") == 0) return C_TOKEN_INTMAX_T;
    if (strcmp(str, "uintmax_t") == 0) return C_TOKEN_UINTMAX_T;
    if (strcmp(str, "nullptr_t") == 0) return C_TOKEN_NULLPTR_T;
    if (strcmp(str, "max_align_t") == 0) return C_TOKEN_MAX_ALIGN_T;
    if (strcmp(str, "_Bool") == 0) return C_TOKEN_BOOL_KA;
    if (strcmp(str, "_Complex") == 0 || strcmp(str, "complex") == 0) return C_TOKEN_COMPLEX;
    if (strcmp(str, "_Imaginary") == 0 || strcmp(str, "imaginary") == 0) return C_TOKEN_IMAGINARY;
    if (strcmp(str, "true") == 0) return C_TOKEN_TRUE;
    if (strcmp(str, "false") == 0) return C_TOKEN_FALSE;
    if (strcmp(str, "NULL") == 0) return C_TOKEN_NULL;
    if (strcmp(str, "nullptr") == 0) return C_TOKEN_NULLPTR;
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

    long long val = 0;

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

    // Suffixes
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

        // Skip comments
        if (peek(l) == '/' && l->src[l->pos + 1] == '/') {
            skip_comment(l);
            continue;
        }
        if (peek(l) == '/' && l->src[l->pos + 1] == '*') {
            skip_comment(l);
            continue;
        }

        // Preprocessor directive
        if (peek(l) == '#') {
            advance(l); // consume #
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

            if (strcmp(buf, "define") == 0) {
                skip_to_line_end(l);
                continue;
            } else if (strcmp(buf, "ifdef") == 0 || strcmp(buf, "ifndef") == 0 || strcmp(buf, "if") == 0) {
                skip_to_line_end(l);
                continue;
            } else if (strcmp(buf, "else") == 0 || strcmp(buf, "elif") == 0) {
                skip_to_line_end(l);
                continue;
            } else if (strcmp(buf, "endif") == 0) {
                skip_to_line_end(l);
                continue;
            } else if (strcmp(buf, "undef") == 0) {
                skip_to_line_end(l);
                continue;
            } else {
                // Unknown preprocessor directive, skip line
                skip_to_line_end(l);
                continue;
            }
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
        t.type = C_TOKEN_EOF;
        return t;
    }

    // Identifier or keyword
    if (isalpha((unsigned char)c) || c == '_') {
        return c_lex_identifier(l, advance(l));
    }

    // Number
    if (isdigit((unsigned char)c)) {
        return c_lex_number(l, advance(l));
    }

    // String literal
    if (c == '"') {
        return c_lex_string(l);
    }

    // Character literal
    if (c == '\'') {
        return c_lex_char(l);
    }

    // Two-character operators
    if (c == '-' && l->src[l->pos + 1] == '>') {
        advance(l); advance(l);
        t.type = C_TOKEN_ARROW;
        l->col += 2;
        return t;
    }
    if (c == '-' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_MINUS_ASSIGN;
        l->col += 2;
        return t;
    }
    if (c == '+' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_PLUS_ASSIGN;
        l->col += 2;
        return t;
    }
    if (c == '*' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_STAR_ASSIGN;
        l->col += 2;
        return t;
    }
    if (c == '/' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_SLASH_ASSIGN;
        l->col += 2;
        return t;
    }
    if (c == '%' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_MOD_ASSIGN;
        l->col += 2;
        return t;
    }
    if (c == '&' && l->src[l->pos + 1] == '&') {
        advance(l); advance(l);
        t.type = C_TOKEN_AND;
        l->col += 2;
        return t;
    }
    if (c == '|' && l->src[l->pos + 1] == '|') {
        advance(l); advance(l);
        t.type = C_TOKEN_OR;
        l->col += 2;
        return t;
    }
    if (c == '=' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_EQ;
        l->col += 2;
        return t;
    }
    if (c == '!' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_NEQ;
        l->col += 2;
        return t;
    }
    if (c == '<' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_LTE;
        l->col += 2;
        return t;
    }
    if (c == '>' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_GTE;
        l->col += 2;
        return t;
    }
    if (c == '<' && l->src[l->pos + 1] == '<') {
        advance(l); advance(l);
        t.type = C_TOKEN_LSHIFT;
        l->col += 2;
        return t;
    }
    if (c == '>' && l->src[l->pos + 1] == '>') {
        advance(l); advance(l);
        t.type = C_TOKEN_RSHIFT;
        l->col += 2;
        return t;
    }
    if (c == '&' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_AND_ASSIGN;
        l->col += 2;
        return t;
    }
    if (c == '|' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_OR_ASSIGN;
        l->col += 2;
        return t;
    }
    if (c == '^' && l->src[l->pos + 1] == '=') {
        advance(l); advance(l);
        t.type = C_TOKEN_XOR_ASSIGN;
        l->col += 2;
        return t;
    }
    if (c == '-' && l->src[l->pos + 1] == '-') {
        advance(l); advance(l);
        t.type = C_TOKEN_DECREMENT;
        l->col += 2;
        return t;
    }
    if (c == '+' && l->src[l->pos + 1] == '+') {
        advance(l); advance(l);
        t.type = C_TOKEN_INCREMENT;
        l->col += 2;
        return t;
    }
    if (c == '.' && l->src[l->pos + 1] == '.') {
        if (l->src[l->pos + 2] == '.') {
            advance(l); advance(l); advance(l);
            t.type = C_TOKEN_ELLIPSIS;
            l->col += 3;
            return t;
        }
        advance(l); advance(l);
        t.type = C_TOKEN_UNKNOWN;
        l->col += 2;
        return t;
    }
    if (c == '.' && l->src[l->pos + 1] == '*') {
        advance(l); advance(l);
        t.type = C_TOKEN_DOT_STAR;
        l->col += 2;
        return t;
    }
    if (c == '-' && l->src[l->pos + 1] == '>' && l->src[l->pos + 2] == '*') {
        advance(l); advance(l); advance(l);
        t.type = C_TOKEN_ARROW_STAR;
        l->col += 3;
        return t;
    }
    if (c == '<' && l->src[l->pos + 1] == '%') {
        advance(l); advance(l);
        t.type = C_TOKEN_LSHIFT;
        l->col += 2;
        return t;
    }
    if (c == '%' && l->src[l->pos + 1] == '>') {
        advance(l); advance(l);
        t.type = C_TOKEN_RSHIFT;
        l->col += 2;
        return t;
    }

    // Single-character operators
    advance(l);
    l->col++;
    switch (c) {
        case ',': t.type = C_TOKEN_COMMA; break;
        case ':': t.type = C_TOKEN_COLON; break;
        case ';': t.type = C_TOKEN_SEMICOLON; break;
        case '(': t.type = C_TOKEN_LPAREN; break;
        case ')': t.type = C_TOKEN_RPAREN; break;
        case '[': t.type = C_TOKEN_LBRACKET; break;
        case ']': t.type = C_TOKEN_RBRACKET; break;
        case '{': t.type = C_TOKEN_LBRACE; break;
        case '}': t.type = C_TOKEN_RBRACE; break;
        case '.': t.type = C_TOKEN_DOT; break;
        case '&': t.type = C_TOKEN_AMPERSAND; break;
        case '*': t.type = C_TOKEN_STAR; break;
        case '+': t.type = C_TOKEN_PLUS; break;
        case '-': t.type = C_TOKEN_MINUS; break;
        case '/': t.type = C_TOKEN_SLASH; break;
        case '%': t.type = C_TOKEN_PERCENT; break;
        case '=': t.type = C_TOKEN_ASSIGN; break;
        case '<': t.type = C_TOKEN_LT; break;
        case '>': t.type = C_TOKEN_GT; break;
        case '!': t.type = C_TOKEN_NOT; break;
        case '~': t.type = C_TOKEN_TILDE; break;
        case '^': t.type = C_TOKEN_XOR; break;
        case '?': t.type = C_TOKEN_QUESTION; break;
        default: t.type = C_TOKEN_UNKNOWN; break;
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
