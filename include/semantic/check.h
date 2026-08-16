/**
 * @file check.h
 * @brief Semantic checking utilities.
 */
#ifndef SEMANTIC_CHECK_H
#define SEMANTIC_CHECK_H

/**
 * @brief Injects default class constructor arguments.
 * @param ctx The semantic context.
 * @param node The call node.
 * @param sym The constructor symbol.
 * @param arg_count The number of provided arguments.
 * @param total_fields The total number of class fields.
 */
void sem_inject_default_class_args(SemanticCtx *ctx, CallNode *node, SemSymbol *sym, int arg_count, int total_fields);

#endif // SEMANTIC_CHECK_H
