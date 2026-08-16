#include "emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../common/arena.h"
#include "../common/context.h"

/**
 * @brief Consumes the lexer and returns a string representation of all tokens.
 * @param l The lexer instance.
 * @return A newly allocated string containing the token dump.
 */
char* lexer_to_string(Lexer *l) {
    StringBuilder sb;
    sb_init(&sb, l->ctx->arena);

    Token t = lexer_next(l);
    while (t.type != TOKEN_EOF) {
        const char *type_str = token_type_to_string(t.type);
        
        sb_append_fmt(&sb, "[%-15s] ", type_str);
        
        // Value info
        if (t.text) {
            sb_append_fmt(&sb, "'%s'", t.text);
        } else if (t.type == TOKEN_NUMBER) {
            sb_append_fmt(&sb, "%d", t.int_val);
        } else if (t.type == TOKEN_SINGLE_LIT) {
            sb_append_fmt(&sb, "%f", t.double_val);
        } else if (t.type == TOKEN_STRING) {
            sb_append_fmt(&sb, "\"%s\"", t.text ? t.text : "");
        }

        sb_append_fmt(&sb, "\t(Line: %d, Col: %d)\n", t.line, t.col);
        
        t = lexer_next(l);
    }
    
    return sb.data;
}

/**
 * @brief Consumes the lexer and writes the string representation to a file.
 * @param l The lexer instance.
 * @param filename The path to the output file.
 */
void lexer_to_file(Lexer *l, const char *filename) {
    char *str = lexer_to_string(l);
    if (str) {
        FILE *f = fopen(filename, "w");
        if (f) {
            fputs(str, f);
            fclose(f);
        }
    }
}

/**
 * @brief Helper that initializes a temporary lexer, converts to string, and cleans up.
 * @param src The source string to lex.
 * @return A newly allocated string containing the token dump.
 */
char* lexer_string_to_string(const char *src) {
    Arena arena;
    arena_init(&arena);
    
    CompilerContext ctx;
    context_init(&ctx, &arena);

    Lexer l;
    lexer_init(&l, &ctx, "", src, NULL);
    
    char* result = lexer_to_string(&l);
    
    arena_free(&arena);
    
    return result;
}

/**
 * @brief Helper that initializes a temporary lexer, writes to file, and cleans up.
 * @param src The source string to lex.
 * @param filename The path to the output file.
 */
void lexer_string_to_file(const char *src, const char *filename) {
    Arena arena;
    arena_init(&arena);
    
    CompilerContext ctx;
    context_init(&ctx, &arena);

    Lexer l;
    lexer_init(&l, &ctx, "", src, NULL);
    
    lexer_to_file(&l, filename);
    
    arena_free(&arena);
}
