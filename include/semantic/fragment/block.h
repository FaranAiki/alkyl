/**
 * @file block.h
 * @brief Block and statement checking declarations.
 */
#ifndef SEMANTIC_FRAGMENT_BLOCK_H
#define SEMANTIC_FRAGMENT_BLOCK_H

/**
 * @brief Checks a single statement.
 * @param ctx The semantic context.
 * @param node The statement AST node.
 */
void sem_check_stmt(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Checks a block of statements.
 * @param ctx The semantic context.
 * @param block The block AST node.
 */
void sem_check_block(SemanticCtx *ctx, ASTNode *block);

/**
 * @brief Checks a generic AST node.
 * @param ctx The semantic context.
 * @param node The AST node.
 */
void sem_check_node(SemanticCtx *ctx, ASTNode *node);

#endif // SEMANTIC_FRAGMENT_BLOCK_H
