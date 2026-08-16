#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

#include "../lexer/lexer.h"
#include "context.h"

/**
 * @brief Sets the current diagnostic namespace.
 * @param ctx The compiler context.
 * @param ns The namespace name to set.
 */
void diag_set_namespace(CompilerContext *ctx, const char *ns);
/**
 * @brief Retrieves the current diagnostic namespace.
 * @param ctx The compiler context.
 * @return The current namespace name, or "unknown" if ctx is NULL.
 */
const char* diag_get_namespace(CompilerContext *ctx);

/**
 * @brief Reports a detailed error with source snippet.
 * @param l The lexer instance (contains context pointer).
 * @param t The token associated with the error.
 * @param msg The error message.
 */
void report_error(Lexer *l, Token t, const char *msg);

/**
 * @brief Reports a warning (magenta/purple).
 * @param l The lexer instance.
 * @param t The token associated with the warning.
 * @param msg The warning message.
 */
void report_warning(Lexer *l, Token t, const char *msg);

/**
 * @brief Reports a hint message (yellow).
 * @param l The lexer instance.
 * @param t The token associated with the hint.
 * @param msg The hint message.
 */
void report_hint(Lexer *l, Token t, const char *msg);

#include "../lexer/c_lexer.h"

/**
 * @brief Reports a C header parser error.
 * @param l The C lexer instance.
 * @param t The token associated with the error.
 * @param msg The error message.
 */
void report_c_error(CLexer *l, CToken t, const char *msg);

/**
 * @brief Reports an informational message (blue).
 * @param l The lexer instance.
 * @param t The token associated with the info.
 * @param msg The info message.
 */
void report_info(Lexer *l, Token t, const char *msg);

/**
 * @brief Reports a reason message (purple).
 * @param l The lexer instance.
 * @param t The token associated with the reason.
 * @param msg The reason message.
 */
void report_reason(Lexer *l, Token t, const char *msg);

#include "../parser/parser.h"

/**
 * @brief Converts a token type to its human-readable string representation.
 * @param type The token type.
 * @return A string describing the token type.
 */
const char* token_type_to_string(TokenType type);
/**
 * @brief Returns a short description of a token type for display.
 * @param type The token type.
 * @return A short string describing the token.
 */
const char* get_token_description(TokenType type);
/**
 * @brief Finds the closest keyword to an identifier using Levenshtein distance.
 * @param ident The identifier to match.
 * @return The closest keyword, or NULL if none is within distance 3.
 */
const char* find_closest_keyword(const char *ident);
/**
 * @brief Computes the Levenshtein distance between two strings.
 * @param s1 First string.
 * @param s2 Second string.
 * @return The edit distance, or 100 on null input.
 */
int levenshtein_dist(const char *s1, const char *s2);

#endif // DIAGNOSTIC_H
