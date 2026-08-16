/**
 * @file emitter.h
 * @brief AST-to-source emitter for the Alkyl parser.
 */
#ifndef PARSER_EMITTER_H
#define PARSER_EMITTER_H

#include "parser.h"
#include "../common/common.h"

/**
 * @brief Traverses the AST and reconstructs source code into a string.
 * @param parser The parser instance.
 * @param root The root AST node.
 * @return A newly allocated string containing the reconstructed source.
 */
char* parser_to_string(Parser *parser, ASTNode *root);

/**
 * @brief Traverses the AST and writes reconstructed source code to a file.
 * @param parser The parser instance.
 * @param root The root AST node.
 * @param filename The output file path.
 */
void parser_to_file(Parser *parser, ASTNode *root, const char *filename);

/**
 * @brief Helper: parses source string into AST, converts to string, frees AST.
 * @param src The source string to parse.
 * @return A newly allocated string containing the reconstructed source.
 */
char* parser_string_to_string(const char *src);

/**
 * @brief Helper: parses source string into AST, writes to file, frees AST.
 * @param src The source string to parse.
 * @param filename The output file path.
 */
void parser_string_to_file(const char *src, const char *filename);

/**
 * @brief Emits a single AST node into a string builder.
 * @param sb The string builder.
 * @param node The AST node to emit.
 * @param indent The current indentation level.
 */
void parser_emit_ast_node(StringBuilder *sb, ASTNode *node, int indent);

#endif // PARSER_EMITTER_H
