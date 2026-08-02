#ifndef SEMANTIC_FIELD_H
#define SEMANTIC_FIELD_H

int sem_count_class_fields(SemanticCtx *ctx, SemSymbol *sym);
int sem_count_required_class_fields(SemanticCtx *ctx, SemSymbol *sym);
void sem_collect_class_fields(SemanticCtx *ctx, SemSymbol *sym, VarDeclNode **fields, int *idx);

#endif // SEMANTIC_FIELD_H
