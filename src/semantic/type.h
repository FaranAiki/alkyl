#ifndef SEMANTIC_TYPE_H
#define SEMANTIC_TYPE_H

#include "semantic.h"

void sem_check_implicit_cast(SemanticCtx *ctx, ASTNode *node, VarType dest, VarType src);
void sem_check_var_decl(SemanticCtx *ctx, VarDeclNode *node, int register_sym);
void sem_check_assign(SemanticCtx *ctx, AssignNode *node);

int is_numeric(VarType t); 
int is_integer(VarType t); 
int is_bool(VarType t); 
int is_pointer(VarType t); 

#endif // SEMANTIC_TYPE_H
