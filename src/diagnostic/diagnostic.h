#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

#include "../lexer/lexer.h"

// Colors for terminal output
#define DIAG_RED    "\033[1;31m"
#define DIAG_GREEN  "\033[1;32m"
#define DIAG_RESET  "\033[0m"
#define DIAG_BOLD   "\033[1m"
#define DIAG_GREY   "\033[0;90m"
#define DIAG_CYAN   "\033[1;36m"

// Report a detailed error with source snippet
void report_error(Lexer *l, Token t, const char *msg);

// Convert a token type to a human-readable string (e.g., TOKEN_SEMICOLON -> ";")
const char* token_type_to_string(TokenType type);

// Helper to hint about missing delimiters
const char* get_token_description(TokenType type);

#endif
