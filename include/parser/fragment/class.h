/**
 * @file class.h
 * @brief Class and enum parsing declarations for the Alkyl parser.
 */
#ifndef ALKYL_PARSER_FRAGMENT_CLASS_H
#define ALKYL_PARSER_FRAGMENT_CLASS_H

#include "parser/typestruct.h"
#include "parser/parser.h"

/**
 * @brief Parses a class declaration.
 * @param p The parser.
 * @return The parsed class AST node.
 */
ASTNode* parse_class(Parser *p);

/**
 * @brief Parses a class implementation.
 * @param p The parser.
 * @param modifiers Modifier flags.
 * @return The parsed class AST node.
 */
ASTNode* parse_class_impl(Parser *p, int modifiers);

/**
 * @brief Parses an enum declaration.
 * @param p The parser.
 * @return The parsed enum AST node.
 */
ASTNode* parse_enum(Parser *p);

#endif // ALKYL_PARSER_FRAGMENT_CLASS_H
