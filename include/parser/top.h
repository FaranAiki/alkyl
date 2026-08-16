/**
 * @file top.h
 * @brief Top-level parsing routines for the Alkyl parser.
 */
#ifndef PARSER_TOP_H
#define PARSER_TOP_H

#include "parser_internal.h"

/**
 * @brief Parses a single top-level declaration.
 * @param p The parser.
 * @return The parsed top-level AST node.
 */
ASTNode* parse_top_level(Parser *p); 

/**
 * @brief Applies implicit return to the last expression in a function body.
 * @param p The parser.
 * @param body_head_ptr Pointer to the head of the body statement list.
 */
void apply_implicit_return(Parser *p, ASTNode **body_head_ptr);

/**
 * @brief Parses an enum declaration.
 * @param p The parser.
 * @return The parsed enum AST node.
 */
ASTNode* parse_enum(Parser *p);

/**
 * @brief Parses a class declaration.
 * @param p The parser.
 * @return The parsed class AST node.
 */
ASTNode* parse_class(Parser *p);

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

/**
 * @brief Parses a link declaration.
 * @param p The parser.
 * @return The parsed link AST node.
 */
ASTNode* parse_link(Parser *p);

/**
 * @brief Parses an extern declaration.
 * @param p The parser.
 * @param modifier Modifier flags.
 * @return The parsed extern AST node.
 */
ASTNode* parse_extern(Parser *p, int modifier);

#endif // PARSER_TOP_H
