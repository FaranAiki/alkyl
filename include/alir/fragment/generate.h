/**
 * @file generate.h
 * @brief ALIR statement generation helpers.
 */
#ifndef ALIR_FRAGMENT_GENERATE_H
#define ALIR_FRAGMENT_GENERATE_H

/**
 * @brief Generates ALIR for a variable declaration statement.
 * @param ctx The ALIR context.
 * @param node The variable declaration AST node.
 */
void alir_stmt_vardecl(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Generates ALIR for an assignment statement.
 * @param ctx The ALIR context.
 * @param node The assignment AST node.
 */
void alir_stmt_assign(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Generates ALIR for a while statement.
 * @param ctx The ALIR context.
 * @param node The while AST node.
 */
void alir_stmt_while(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Generates ALIR for a for-in statement.
 * @param ctx The ALIR context.
 * @param node The for-in AST node.
 */
void alir_stmt_for_in(AlirCtx *ctx, ASTNode *node);

#endif // ALIR_FRAGMENT_GENERATE_H
