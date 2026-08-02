#ifndef SEMANTIC_TAINT_H
#define SEMANTIC_TAINT_H
#include "semantic.h"

SemSymbol* sem_get_errnum_func_sym(SemanticCtx *ctx, ASTNode *node);
void sem_check_residue_exhaustive(SemanticCtx *ctx, ASTNode *where,
                                         SemSymbol *err_sym, ResidueCase *cases,
                                         int default_case);

#endif // SEMANTIC_TAINT_H
