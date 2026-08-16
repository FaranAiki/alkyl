#ifndef LEXER_EMITTER_H
#define LEXER_EMITTER_H

#include "lexer.h"
#include "../common/common.h"

/**
 * @brief Consumes the lexer and returns a string representation of all tokens.
 * @param l The lexer instance.
 * @return A newly allocated string containing the token dump.
 */
char* lexer_to_string(Lexer *l);

/**
 * @brief Consumes the lexer and writes the string representation to a file.
 * @param l The lexer instance.
 * @param filename The path to the output file.
 */
void lexer_to_file(Lexer *l, const char *filename);

/**
 * @brief Helper: initializes a temporary lexer with src, converts to string, cleans up.
 * @param src The source string to lex.
 * @return A newly allocated string containing the token dump.
 */
char* lexer_string_to_string(const char *src);

/**
 * @brief Helper: initializes a temporary lexer with src, writes to file, cleans up.
 * @param src The source string to lex.
 * @param filename The path to the output file.
 */
void lexer_string_to_file(const char *src, const char *filename);

#include "../common/diagnostic.h"

#endif // LEXER_EMITTER_H
