#ifndef SEMANTIC_CHECK_H
#define SEMANTIC_CHECK_H

void sem_inject_default_class_args(SemanticCtx *ctx, CallNode *node, SemSymbol *sym, int arg_count, int total_fields);

#endif // SEMANTIC_CHECK_H
