/**
 * @file taint.h
 * @brief Taint and residue checking declarations.
 */
#ifndef SEMANTIC_TAINT_H
#define SEMANTIC_TAINT_H
#include "semantic.h"

/**
 * @brief Gets the errnum function symbol for a node.
 * @param ctx The semantic context.
 * @param node The AST node.
 * @return The errnum function symbol, or NULL.
 */
SemSymbol* sem_get_errnum_func_sym(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Checks that residue cases are exhaustive.
 * @param ctx The semantic context.
 * @param where The AST node where the check occurs.
 * @param err_sym The error symbol.
 * @param cases The residue cases.
 * @param default_case Whether there is a default case.
 */
void sem_check_residue_exhaustive(SemanticCtx *ctx, ASTNode *where,
                                          SemSymbol *err_sym, ResidueCase *cases,
                                          int default_case);

#endif // SEMANTIC_TAINT_H
