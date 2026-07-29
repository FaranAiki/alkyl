#ifndef ALIR_GENERATOR_H 
#define ALIR_GENERATOR_H

#include "alir.h"

void push_loop(AlirCtx *ctx, AlirBlock *cont, AlirBlock *brk);
void pop_loop(AlirCtx *ctx);
int is_terminator(AlirOpcode op); 

long alir_eval_constant_int(AlirCtx *ctx, ASTNode *node);
AlirValue* alir_fold_const_expr(AlirCtx *ctx, ASTNode *node, VarType target);
void scan_and_fold_consts(AlirCtx *ctx, ASTNode *root);
// TODO change this class node 
void build_struct_fields(AlirCtx *ctx, ClassNode *cn, AlirStruct *st);
void pass1_register(AlirCtx *ctx, ASTNode *n, const char *current_ns);
void pass2_populate(AlirCtx *ctx, ASTNode *root, ASTNode *n, const char *current_ns);
void alir_scan_and_register_classes(AlirCtx *ctx, ASTNode *root);
void alir_gen_switch(AlirCtx *ctx, SwitchNode *sn);
void alir_gen_implicit_constructor(AlirCtx *ctx, ClassNode *cn, const char *fqn);
void alir_gen_inherited_methods(AlirCtx *ctx, ClassNode *cn, const char *class_name);
void alir_gen_functions_recursive(AlirCtx *ctx, ASTNode *root, const char *current_ns);
void alir_gen_function_def(AlirCtx *ctx, FuncDefNode *fn, const char *class_name);
AlirModule* alir_generate(SemanticCtx *sem, ASTNode *root);

#endif // ALIR_GENERATOR_H
