/**
 * @file stmt.h
 * @brief ALIR statement generation declarations.
 */
#ifndef ALIR_STMT_H
#define ALIR_STMT_H

#include "alir.h"

/**
 * @brief Generates ALIR for a generic expression.
 * @param ctx The ALIR context.
 * @param node The expression AST node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_expr(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Generates ALIR for a generic statement.
 * @param ctx The ALIR context.
 * @param node The statement AST node.
 */
void alir_gen_stmt(AlirCtx *ctx, ASTNode *node);

#endif // ALIR_STMT_H
