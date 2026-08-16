/**
 * @file type.h
 * @brief Type checking declarations for semantic analysis.
 */
#ifndef SEMANTIC_TYPE_H
#define SEMANTIC_TYPE_H

#include "semantic.h"

/**
 * @brief Checks implicit cast compatibility.
 * @param ctx The semantic context.
 * @param node The AST node.
 * @param dest The destination type.
 * @param src The source type.
 */
void sem_check_implicit_cast(SemanticCtx *ctx, ASTNode *node, VarType dest, VarType src);

/**
 * @brief Checks a variable declaration.
 * @param ctx The semantic context.
 * @param node The variable declaration node.
 * @param register_sym Whether to register the symbol.
 */
void sem_check_var_decl(SemanticCtx *ctx, VarDeclNode *node, int register_sym);

/**
 * @brief Checks an assignment.
 * @param ctx The semantic context.
 * @param node The assignment node.
 */
void sem_check_assign(SemanticCtx *ctx, AssignNode *node);

/**
 * @brief Checks if a type is numeric.
 * @param t The type.
 * @return Non-zero if numeric.
 */
int is_numeric(VarType t); 

/**
 * @brief Checks if a type is integer.
 * @param t The type.
 * @return Non-zero if integer.
 */
int is_integer(VarType t); 

/**
 * @brief Checks if a type is bool.
 * @param t The type.
 * @return Non-zero if bool.
 */
int is_bool(VarType t); 

/**
 * @brief Checks if a type is a pointer.
 * @param t The type.
 * @return Non-zero if pointer.
 */
int is_pointer(VarType t); 

#endif // SEMANTIC_TYPE_H
