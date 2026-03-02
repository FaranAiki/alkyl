#ifndef SEMANTIC_MODIFIER_FUNC_H
#define SEMANTIC_MODIFIER_FUNC_H

#include "semantic.h"

void sem_check_func_def(SemanticCtx *ctx, FuncDefNode *node);
void sem_check_method_call(SemanticCtx *ctx, MethodCallNode *node);

#endif // SEMANTIC_MODIFIER_FUNC_H
