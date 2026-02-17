#ifndef ALIR_LVALUE_H
#define ALIR_LVALUE_H

#include "alir.h"

AlirValue* alir_gen_addr(AlirCtx *ctx, ASTNode *node);
AlirValue* alir_gen_trait_access(AlirCtx *ctx, TraitAccessNode *ta);
AlirValue* alir_gen_literal(AlirCtx *ctx, LiteralNode *ln);
AlirValue* alir_gen_var_ref(AlirCtx *ctx, VarRefNode *vn);
AlirValue* alir_gen_access(AlirCtx *ctx, ASTNode *node);
AlirValue* alir_gen_binary_op(AlirCtx *ctx, BinaryOpNode *bn);
AlirValue* alir_gen_call_std(AlirCtx *ctx, CallNode *cn);
AlirValue* alir_gen_call(AlirCtx *ctx, CallNode *cn);
AlirValue* alir_gen_method_call(AlirCtx *ctx, MethodCallNode *mc);
AlirValue* alir_gen_expr(AlirCtx *ctx, ASTNode *node);

#endif // ALIR_LVALUE_H
