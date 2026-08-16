/**
 * @file lookup.h
 * @brief Symbol lookup declarations for semantic analysis.
 */
#ifndef SEMANTIC_FRAGMENT_LOOKUP_H
#define SEMANTIC_FRAGMENT_LOOKUP_H 

#include "../semantic.h"

/**
 * @brief Looks up a class method call.
 * @param ctx The semantic context.
 * @param node The method call node.
 * @return Non-zero if the lookup succeeded.
 */
int sem_lookup_class_call(SemanticCtx *ctx, MethodCallNode *node);

#endif // SEMANTIC_FRAGMENT_LOOKUP_H
