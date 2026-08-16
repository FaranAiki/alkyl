/**
 * @file switch.h
 * @brief Switch and for-in checking declarations.
 */
#ifndef SEMANTIC_FRAGMENT_SWITCH_H
#define SEMANTIC_FRAGMENT_SWITCH_H 

#include "../semantic.h"

/**
 * @brief Checks a for-in statement.
 * @param ctx The semantic context.
 * @param node The for-in AST node.
 */
void sem_check_for_in(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Checks a unary operator in a switch context.
 * @param ctx The semantic context.
 * @param node The unary operator AST node.
 */
void sem_check_unary_op_switch(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Checks a variable reference.
 * @param ctx The semantic context.
 * @param node The variable reference AST node.
 */
void sem_check_var_ref(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Checks an index access expression.
 * @param ctx The semantic context.
 * @param node The index access AST node.
 */
void sem_check_index_access(SemanticCtx *ctx, ASTNode *node);

#endif // SEMANTIC_FRAGMENT_SWITCH_H
