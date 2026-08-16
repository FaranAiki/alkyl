/**
 * @file cond.h
 * @brief Conditional parsing declarations for the Alkyl parser.
 */
#ifndef ALKYL_PARSER_FRAGMENT_COND_H
#define ALKYL_PARSER_FRAGMENT_COND_H

#include "parser/typestruct.h"
#include "parser/parser.h"

/**
 * @brief Parses an if statement.
 * @param p The parser.
 * @return The parsed if AST node.
 */
ASTNode* parse_if(Parser *p);

/**
 * @brief Parses a switch statement.
 * @param p The parser.
 * @return The parsed switch AST node.
 */
ASTNode* parse_switch(Parser *p);

#endif // ALKYL_PARSER_FRAGMENT_COND_H
