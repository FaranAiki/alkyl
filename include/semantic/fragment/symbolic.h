/**
 * @file symbolic.h
 * @brief Symbolic execution declarations for semantic analysis.
 */
#ifndef SEMANTIC_FRAGMENT_SYMBOLIC_H
#define SEMANTIC_FRAGMENT_SYMBOLIC_H

#include "semantic.h"

/**
 * @brief Runs symbolic execution on a function definition.
 * @param ctx The semantic context.
 * @param node The function definition AST node.
 */
void sem_symbolic_func_def(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Runs symbolic execution on a variable declaration.
 * @param ctx The semantic context.
 * @param node The variable declaration AST node.
 */
void sem_symbolic_var_decl(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Runs symbolic execution on an enum node.
 * @param ctx The semantic context.
 * @param node The enum AST node.
 */
void sem_symbolic_node_enum(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Runs symbolic execution on a namespace.
 * @param ctx The semantic context.
 * @param node The namespace AST node.
 */
void sem_symbolic_namespace(SemanticCtx *ctx, ASTNode *node);

#endif // SEMANTIC_FRAGMENT_SYMBOLIC_H
