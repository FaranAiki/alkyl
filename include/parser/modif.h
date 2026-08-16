/**
 * @file modif.h
 * @brief Macro and typedef parsing declarations for the Alkyl parser.
 */
#ifndef PARSER_MODIF_H
#define PARSER_MODIF_H

#include "parser_internal.h"

typedef struct MacroSig {
    char *name;
    char **params;
    int param_count;
} MacroSig;

/**
 * @brief Parses a define (macro) declaration.
 * @param p The parser.
 * @return The parsed define AST node.
 */
ASTNode* parse_define(Parser *p);

/**
 * @brief Parses a typedef declaration.
 * @param p The parser.
 * @return The parsed typedef AST node.
 */
ASTNode* parse_typedef(Parser *p);

#endif // PARSER_MODIF_H
