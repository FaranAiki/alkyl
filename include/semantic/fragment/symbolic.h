#ifndef SEMANTIC_FRAGMENT_SYMBOLIC_H
#define SEMANTIC_FRAGMENT_SYMBOLIC_H

#include "semantic.h"

void sem_symbolic_func_def(SemanticCtx *ctx, ASTNode *node);

void sem_symbolic_var_decl(SemanticCtx *ctx, ASTNode *node);

void sem_symbolic_node_enum(SemanticCtx *ctx, ASTNode *node);

void sem_symbolic_namespace(SemanticCtx *ctx, ASTNode *node);

#endif // SEMANTIC_FRAGMENT_SYMBOLIC_H
