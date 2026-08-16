/**
 * @file loop.h
 * @brief Loop parsing declarations for the Alkyl parser.
 */
#ifndef ALKYL_PARSER_FRAGMENT_LOOP_H
#define ALKYL_PARSER_FRAGMENT_LOOP_H

#include "parser/typestruct.h"
#include "parser/parser.h"

/**
 * @brief Parses a while statement.
 * @param p The parser.
 * @return The parsed while AST node.
 */
ASTNode* parse_while(Parser *p);

/**
 * @brief Parses a loop statement.
 * @param p The parser.
 * @return The parsed loop AST node.
 */
ASTNode* parse_loop(Parser *p);

#endif // ALKYL_PARSER_FRAGMENT_LOOP_H
