#include "alir.h"

AlirValue* alir_gen_binary_op(AlirCtx *ctx, BinaryOpNode *bn) {
    if (bn->overloaded_func_name) {
        // Emit as function call
        AlirValue *l = alir_gen_expr(ctx, bn->left);
        AlirValue *r = alir_gen_expr(ctx, bn->right);

        AlirValue **args = arena_alloc(ctx->sem->compiler_ctx->arena, 2 * sizeof(AlirValue*));
        args[0] = l;
        args[1] = r;

        VarType res_ty = sem_get_node_type(ctx->sem, (ASTNode*)bn);
        AlirValue *res = NULL;
        if (res_ty.base != TYPE_VOID || res_ty.ptr_depth > 0) {
            res = new_temp(ctx, res_ty);
        }

        AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, res, alir_val_var(ctx->module, bn->overloaded_func_name), NULL);
        call->args = args;
        call->arg_count = 2;
        emit(ctx, call);
        return res;
    }

    AlirValue *l = alir_gen_expr(ctx, bn->left);
    AlirValue *r = alir_gen_expr(ctx, bn->right);

    if (!l) {
        l = new_temp(ctx, (VarType){TYPE_INT, 0});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, l, NULL, NULL));
    }
    if (!r) {
        r = new_temp(ctx, (VarType){TYPE_INT, 0});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, r, NULL, NULL));
    }

    // Check types via Semantic Context to decide on Float vs Int ops
    VarType l_type = sem_get_node_type(ctx->sem, bn->left);
    VarType r_type = sem_get_node_type(ctx->sem, bn->right);

    // Fallback operator
    if (bn->op == TOKEN_QUESTION || bn->op == TOKEN_QUESTION_QUESTION) {
        VarType res_ty = sem_get_node_type(ctx->sem, (ASTNode*)bn);
        AlirValue *res = new_temp(ctx, res_ty);
        AlirInst *inst = mk_inst(ctx->module, ALIR_OP_FALLBACK, res, l, r);
        if (bn->fallback_err_name) {
            void *err_val = hashmap_get(&ctx->sem->compiler_ctx->error_table, bn->fallback_err_name);
            if (err_val) {
                int id = (int)(intptr_t)err_val;
                AlirValue *id_val = alir_const_int(ctx->module, id);
                inst->args = arena_alloc(ctx->sem->compiler_ctx->arena, sizeof(AlirValue*));
                inst->args[0] = id_val;
                inst->arg_count = 1;
            }
        }
        emit(ctx, inst);
        return res;
    }

    int is_float = (l_type.base == TYPE_SINGLE || l_type.base == TYPE_DOUBLE ||
                    r_type.base == TYPE_SINGLE || r_type.base == TYPE_DOUBLE ||
                    l_type.base == TYPE_LONG_DOUBLE || r_type.base == TYPE_LONG_DOUBLE);

    VarType res_type = (VarType){TYPE_INT, 0};

    if (is_float) {
        VarType target = {TYPE_DOUBLE, 0};

        if (l_type.base == TYPE_DOUBLE || r_type.base == TYPE_DOUBLE) {
            target.base = TYPE_DOUBLE;
        } else if (l_type.base == TYPE_SINGLE || r_type.base == TYPE_SINGLE) {
            target.base = TYPE_SINGLE;
        }

        l = promote(ctx, l, target);
        r = promote(ctx, r, target);
        res_type = target;
    } else {
        if (l_type.base != TYPE_UNKNOWN && r_type.base != TYPE_UNKNOWN && l_type.ptr_depth == 0 && r_type.ptr_depth == 0 && !l_type.is_func_ptr && !r_type.is_func_ptr) {
            if (l_type.base > r_type.base) {
                r = promote(ctx, r, l_type);
            } else if (l_type.base < r_type.base) {
                l = promote(ctx, l, r_type);
            }
        }
    }

    AlirOpcode op = ALIR_OP_ADD;
    switch(bn->op) {
        case TOKEN_PLUS: op = is_float ? ALIR_OP_FADD : ALIR_OP_ADD; break;
        case TOKEN_MINUS: op = is_float ? ALIR_OP_FSUB : ALIR_OP_SUB; break;
        case TOKEN_STAR: op = is_float ? ALIR_OP_FMUL : ALIR_OP_MUL; break;
        case TOKEN_SLASH: op = is_float ? ALIR_OP_FDIV : ALIR_OP_DIV; break;
        case TOKEN_EQ: op = ALIR_OP_EQ; break;
        case TOKEN_LT: op = ALIR_OP_LT; break;
        case TOKEN_GT: op = ALIR_OP_GT; break;
        case TOKEN_LTE: op = ALIR_OP_LTE; break;
        case TOKEN_GTE: op = ALIR_OP_GTE; break;
        case TOKEN_NEQ: op = ALIR_OP_NEQ; break;
        case TOKEN_AND: op = ALIR_OP_AND; break;
        case TOKEN_OR: op = ALIR_OP_OR; break;
        case TOKEN_XOR: op = ALIR_OP_XOR; break;
        case TOKEN_LSHIFT: op = ALIR_OP_SHL; break;
        case TOKEN_RSHIFT: op = ALIR_OP_SHR; break;
        case TOKEN_LROTATE: op = ALIR_OP_ROTL; break;
        case TOKEN_RROTATE: op = ALIR_OP_ROTR; break;
        case TOKEN_MOD: op = ALIR_OP_MOD; break;
        // ... add other cases
    }

    // Result type logic
    if (op == ALIR_OP_EQ || op == ALIR_OP_LT || op == ALIR_OP_GT || op == ALIR_OP_LTE || op == ALIR_OP_GTE || op == ALIR_OP_NEQ) res_type = (VarType){TYPE_BOOL, 0};

    if (l->kind == ALIR_VAL_CONST && r->kind == ALIR_VAL_CONST) {
        if (l->type.base == TYPE_DOUBLE || r->type.base == TYPE_DOUBLE) {
            double lv = l->val.double_val;
            double rv = r->val.double_val;
            if (op == ALIR_OP_EQ) return alir_const_int(ctx->module, lv == rv ? 1 : 0);
            if (op == ALIR_OP_NEQ) return alir_const_int(ctx->module, lv != rv ? 1 : 0);
            if (op == ALIR_OP_LT) return alir_const_int(ctx->module, lv < rv ? 1 : 0);
            if (op == ALIR_OP_GT) return alir_const_int(ctx->module, lv > rv ? 1 : 0);
            if (op == ALIR_OP_LTE) return alir_const_int(ctx->module, lv <= rv ? 1 : 0);
            if (op == ALIR_OP_GTE) return alir_const_int(ctx->module, lv >= rv ? 1 : 0);
        } else if (l->type.base == TYPE_SINGLE || r->type.base == TYPE_SINGLE) {
            float lv = l->val.single_val;
            float rv = r->val.single_val;
            if (op == ALIR_OP_EQ) return alir_const_int(ctx->module, lv == rv ? 1 : 0);
            if (op == ALIR_OP_NEQ) return alir_const_int(ctx->module, lv != rv ? 1 : 0);
            if (op == ALIR_OP_LT) return alir_const_int(ctx->module, lv < rv ? 1 : 0);
            if (op == ALIR_OP_GT) return alir_const_int(ctx->module, lv > rv ? 1 : 0);
            if (op == ALIR_OP_LTE) return alir_const_int(ctx->module, lv <= rv ? 1 : 0);
            if (op == ALIR_OP_GTE) return alir_const_int(ctx->module, lv >= rv ? 1 : 0);
        } else {
            if (op == ALIR_OP_EQ) {
                return alir_const_int(ctx->module, l->val.int_val == r->val.int_val ? 1 : 0);
            } else if (op == ALIR_OP_NEQ) {
                return alir_const_int(ctx->module, l->val.int_val != r->val.int_val ? 1 : 0);
            } else if (op == ALIR_OP_LT) {
                return alir_const_int(ctx->module, l->val.int_val < r->val.int_val ? 1 : 0);
            } else if (op == ALIR_OP_GT) {
                return alir_const_int(ctx->module, l->val.int_val > r->val.int_val ? 1 : 0);
            } else if (op == ALIR_OP_LTE) {
                return alir_const_int(ctx->module, l->val.int_val <= r->val.int_val ? 1 : 0);
            } else if (op == ALIR_OP_GTE) {
                return alir_const_int(ctx->module, l->val.int_val >= r->val.int_val ? 1 : 0);
            }
        }
    }

    AlirValue *dest = new_temp(ctx, res_type);
    if (op == ALIR_OP_NEQ) {
        AlirValue *eq = new_temp(ctx, res_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_EQ, eq, l, r));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_NOT, dest, eq, NULL));
    } else if (op == ALIR_OP_LTE) {
        AlirValue *gt = new_temp(ctx, res_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_GT, gt, l, r));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_NOT, dest, gt, NULL));
    } else if (op == ALIR_OP_GTE) {
        AlirValue *lt = new_temp(ctx, res_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_LT, lt, l, r));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_NOT, dest, lt, NULL));
    } else {
        emit(ctx, mk_inst(ctx->module, op, dest, l, r));
    }
    return dest;
}

AlirValue* alir_gen_unary_op(AlirCtx *ctx, UnaryOpNode *un) {
    if (un->overloaded_func_name) {
        // Emit as function call
        AlirValue *operand = alir_gen_expr(ctx, un->operand);

        AlirValue **args = arena_alloc(ctx->sem->compiler_ctx->arena, sizeof(AlirValue*));
        args[0] = operand;

        VarType res_ty = sem_get_node_type(ctx->sem, (ASTNode*)un);
        AlirValue *res = NULL;
        if (res_ty.base != TYPE_VOID || res_ty.ptr_depth > 0) {
            res = new_temp(ctx, res_ty);
        }

        AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, res, alir_val_var(ctx->module, un->overloaded_func_name), NULL);
        call->args = args;
        call->arg_count = 1;
        emit(ctx, call);
        return res;
    }

    AlirValue *operand = alir_gen_expr(ctx, un->operand);
    if (!operand) {
        operand = new_temp(ctx, (VarType){TYPE_INT, 0});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, operand, NULL, NULL));
    }

    AlirOpcode op = ALIR_OP_NOT;
    VarType res_type = sem_get_node_type(ctx->sem, (ASTNode*)un);

    switch(un->op) {
        case TOKEN_MINUS: {
            // Lower unary minus to: 0 - operand
            AlirValue *zero = alir_const_int(ctx->module, 0);
            if (res_type.base == TYPE_SINGLE) {
                zero = alir_const_float(ctx->module, 0.0);
                op = ALIR_OP_FSUB;
            } else if (res_type.base == TYPE_DOUBLE) {
                zero = alir_const_double(ctx->module, 0.0);
                op = ALIR_OP_FSUB;
            } else {
                op = ALIR_OP_SUB;
            }
            AlirValue *dest = new_temp(ctx, res_type);
            emit(ctx, mk_inst(ctx->module, op, dest, zero, operand));
            return dest;
        }
        case TOKEN_NOT: {
            AlirValue *dest = new_temp(ctx, res_type);
            if (operand->type.ptr_depth > 0 || operand->type.base == TYPE_BOOL) {
                emit(ctx, mk_inst(ctx->module, ALIR_OP_NOT, dest, operand, NULL));
            } else {
                AlirValue *zero = alir_const_int(ctx->module, 0);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_EQ, dest, operand, zero));
            }
            return dest;
        }
        case TOKEN_BIT_NOT:
            // ALIR doesn't have an explicit BIT_NOT, usually lowered to XOR -1
            op = ALIR_OP_XOR;
            AlirValue *dest = new_temp(ctx, res_type);
            emit(ctx, mk_inst(ctx->module, op, dest, operand, alir_const_int(ctx->module, -1)));
            return dest;
        case TOKEN_STAR: { // Dereference
            AlirValue *dest = new_temp(ctx, res_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, dest, operand, NULL));
            return dest;
        }
        case TOKEN_AND: { // Address-of
            return alir_gen_addr(ctx, un->operand);
        }
        default:
            break;
    }

    AlirValue *dest = new_temp(ctx, res_type);
    emit(ctx, mk_inst(ctx->module, op, dest, operand, NULL));
    return dest;
}

