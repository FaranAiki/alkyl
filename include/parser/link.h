/**
 * @file link.h
 * @brief Import and linker resolution for the Alkyl parser.
 */
#ifndef PARSER_LINK_H
#define PARSER_LINK_H

#include "parser_internal.h"

/**
 * @brief Parses an import statement.
 * @param p The parser.
 * @return The parsed import AST node.
 */
ASTNode* parse_import(Parser *p);

/**
 * @brief Parses an import from a given filename.
 * @param p The parser.
 * @param fname The file to import.
 * @return The parsed import AST node.
 */
ASTNode* parse_import_internal(Parser *p, const char *fname);

/**
 * @brief Parses a link statement.
 * @param p The parser.
 * @return The parsed link AST node.
 */
ASTNode* parse_link(Parser *p);

/**
 * @brief Resolves all imports in the AST.
 * @param p The parser.
 * @param root_ptr Pointer to the root AST node.
 */
void resolve_imports(Parser *p, ASTNode **root_ptr);

/**
 * @brief Adds pkg-config flags for a library to the compiler context.
 * @param ctx The compiler context.
 * @param lib_name The library name.
 */
void add_pkg_config_flags(CompilerContext *ctx, const char *lib_name);

#endif // PARSER_LINK_H
