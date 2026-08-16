/**
 * @file func.h
 * @brief Function-related semantic checking declarations.
 */
#ifndef SEMANTIC_MODIFIER_FUNC_H
#define SEMANTIC_MODIFIER_FUNC_H

#include "semantic.h"

/**
 * @brief Checks a function definition.
 * @param ctx The semantic context.
 * @param node The function definition node.
 */
void sem_check_func_def(SemanticCtx *ctx, FuncDefNode *node);

/**
 * @brief Checks a method call.
 * @param ctx The semantic context.
 * @param node The method call node.
 */
void sem_check_method_call(SemanticCtx *ctx, MethodCallNode *node);

/**
 * @brief Checks a function call.
 * @param ctx The semantic context.
 * @param node The call node.
 */
void sem_check_call(SemanticCtx *ctx, CallNode *node);

#endif // SEMANTIC_MODIFIER_FUNC_H
