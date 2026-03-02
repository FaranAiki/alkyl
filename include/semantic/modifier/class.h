#ifndef SEMANTIC_MODIFIER_CLASS_H
#define SEMANTIC_MODIFIER_CLASS_H

#include "semantic.h"

void sem_check_member_access(SemanticCtx *ctx, MemberAccessNode *node);
void sem_scan_class_members(SemanticCtx *ctx, ClassNode *cn, SemSymbol *class_sym);

#endif // SEMANTIC_MODIFIER_CLASS_H

