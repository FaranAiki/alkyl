/**
 * @file decl.h
 * @brief Variable declaration parsing for the Alkyl parser.
 */
#ifndef ALKYL_PARSER_FRAGMENT_DECL_H
#define ALKYL_PARSER_FRAGMENT_DECL_H

#include "parser/typestruct.h"
#include "parser/parser.h"

/**
 * @brief Parses a variable declaration.
 * @param p The parser.
 * @return The parsed variable declaration AST node.
 */
ASTNode* parse_var_decl_internal(Parser *p);

#endif // ALKYL_PARSER_FRAGMENT_DECL_H
