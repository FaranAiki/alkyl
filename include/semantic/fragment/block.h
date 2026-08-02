#ifndef SEMANTIC_FRAGMENT_BLOCK_H
#define SEMANTIC_FRAGMENT_BLOCK_H

void sem_check_stmt(SemanticCtx *ctx, ASTNode *node);
void sem_check_block(SemanticCtx *ctx, ASTNode *block);
void sem_check_node(SemanticCtx *ctx, ASTNode *node);

#endif // SEMANTIC_FRAGMENT_BLOCK_H
