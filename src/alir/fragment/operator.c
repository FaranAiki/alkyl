#include "alir.h"

/**
 * @brief Generate IR for a binary operation expression.
 * @param ctx The ALIR context.
 * @param bn The binary operation AST node.
 * @return Result value, or NULL on failure.
 */
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

    // For block-form fallback, bn->right is a statement list -- don't evaluate it as an expression.
    // The block will be generated inline in the fallback operator section below.
    int is_fallback_block = (bn->op == TOKEN_QUESTION || bn->op == TOKEN_QUESTION_QUESTION) && bn->fallback_has_block;

    AlirValue *r = NULL;
    if (!is_fallback_block) {
        r = alir_gen_expr(ctx, bn->right);
    }

    if (!l) {
        l = new_temp(ctx, (VarType){TYPE_INT, 0});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, l, NULL, NULL));
    }
    if (!r && !is_fallback_block) {
        r = new_temp(ctx, (VarType){TYPE_INT, 0});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, r, NULL, NULL));
    }

    // Check types via Semantic Context to decide on Float vs Int ops
    VarType l_type = sem_get_node_type(ctx->sem, bn->left);
    VarType r_type = is_fallback_block ? l_type : sem_get_node_type(ctx->sem, bn->right);


    // Fallback operator
    if (bn->op == TOKEN_QUESTION || bn->op == TOKEN_QUESTION_QUESTION) {
        // Special case: `alir_gen_expr(bn->left)` implicitly unwraps `tainted` NODE_VAR_REF variables,
        // resulting in a plain `bool` instead of `{int, val}`. Fallback operator needs the full struct!
        if (bn->left->type == NODE_VAR_REF) {
            VarRefNode *vn = (VarRefNode*)bn->left;
            AlirSymbol *sym = alir_find_symbol(ctx, vn->name);
            if (sym && sym->type.is_tainted) {
                l = new_temp(ctx, sym->type);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, l, sym->ptr, NULL));
            } else {
                SemSymbol *glob_sym = sem_symbol_lookup(ctx->sem, vn->name, NULL);
                if (glob_sym && glob_sym->type.is_tainted) {
                    AlirValue *ptr = alir_val_global(ctx->module, glob_sym->mangled_name ? glob_sym->mangled_name : vn->name, glob_sym->type);
                    l = new_temp(ctx, glob_sym->type);
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, l, ptr, NULL));
                }
            }
        }

        // Block-form fallback: ? { stmts } / ?? { stmts } / ?[ErrX] { stmts }
        // Generates proper CFG: check error -> enter block -> merge with pristine value.
        if (bn->fallback_has_block) {
            VarType res_ty = sem_get_node_type(ctx->sem, (ASTNode*)bn);

            // `res_ptr` is a stack slot that holds the result value.
            // Both the "no error" path and the fallback body write to it.
            VarType ptr_ty = res_ty;
            ptr_ty.ptr_depth++;
            AlirValue *res_ptr = new_temp(ctx, ptr_ty);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, res_ptr, NULL, NULL));
            ctx->fallback_result_slot = res_ptr;

            // We need a POINTER to the tainted struct for GET_PTR field access.
            // `l` is the loaded value, not a pointer. Find the raw stack/alloca ptr.
            AlirValue *tainted_ptr = NULL;

            if (bn->left->type == NODE_VAR_REF) {
                VarRefNode *vn2 = (VarRefNode*)bn->left;
                AlirSymbol *sym2 = alir_find_symbol(ctx, vn2->name);
                if (sym2 && sym2->type.is_tainted) {
                    tainted_ptr = sym2->ptr;  // ptr to { i32, T } on stack
                } else {
                    SemSymbol *gsym = sem_symbol_lookup(ctx->sem, vn2->name, NULL);
                    if (gsym && gsym->type.is_tainted) {
                        tainted_ptr = alir_val_global(ctx->module,
                            gsym->mangled_name ? gsym->mangled_name : vn2->name, gsym->type);
                    }
                }
            }

            if (!tainted_ptr) {
                // Fallback: alloca a temp, store the loaded value, use its ptr
                VarType taint_ty = l ? l->type : res_ty;
                taint_ty.is_tainted = 1;
                AlirValue *tmp_ptr = new_temp(ctx, taint_ty);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, tmp_ptr, NULL, NULL));
                if (l) emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, l, tmp_ptr));
                tainted_ptr = tmp_ptr;
            }

            // Extract error code from the tainted struct (field 0) via pointer
            AlirValue *err_code_ptr = new_temp(ctx, (VarType){TYPE_INT, 1, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, err_code_ptr, tainted_ptr, alir_const_int(ctx->module, 0)));
            AlirValue *err_code = new_temp(ctx, (VarType){TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, err_code, err_code_ptr, NULL));

            // Build has_err condition
            AlirValue *has_err = new_temp(ctx, (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            if (bn->fallback_err_name) {
                // ?[ErrX] { } -- match a specific error
                void *err_val_ptr = hashmap_get(&ctx->sem->compiler_ctx->error_table, bn->fallback_err_name);
                if (err_val_ptr) {
                    int id = (int)(intptr_t)err_val_ptr;
                    AlirValue *target_id = alir_const_int(ctx->module, id);
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_EQ, has_err, err_code, target_id));
                } else {
                    // Error name not registered yet -- treat as any-error check
                    AlirValue *zero = alir_const_int(ctx->module, 0);
                    AlirValue *neq = new_temp(ctx, (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_EQ, neq, err_code, zero));
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_NOT, has_err, neq, NULL));
                }
            } else {
                // ? { } / ?? { } -- any error
                AlirValue *zero = alir_const_int(ctx->module, 0);
                AlirValue *neq = new_temp(ctx, (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
                emit(ctx, mk_inst(ctx->module, ALIR_OP_EQ, neq, err_code, zero));
                emit(ctx, mk_inst(ctx->module, ALIR_OP_NOT, has_err, neq, NULL));
            }

            // Pre-load the pristine value from the tainted struct and store it into `res_ptr`
            // on the "no error" path. If we take the fallback_body path, `return` inside the
            // block will overwrite `res_ptr` with the return value. At merge, `res_ptr`
            // holds the correct value regardless of which path was taken.
            VarType vptr_ty = res_ty;
            vptr_ty.ptr_depth++;
            AlirValue *val_ptr = new_temp(ctx, vptr_ty);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, val_ptr, tainted_ptr, alir_const_int(ctx->module, 1)));
            AlirValue *pristine_val = new_temp(ctx, res_ty);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, pristine_val, val_ptr, NULL));
            emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, pristine_val, res_ptr));

            // Create blocks: fallback_body, merge
            AlirBlock *fallback_bb = alir_add_block(ctx->module, ctx->current_func, "fallback_body");
            AlirBlock *merge_bb   = alir_add_block(ctx->module, ctx->current_func, "fallback_merge");

            // Branch: if has_err -> fallback_body, else -> merge
            AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, has_err,
                                   alir_val_label(ctx->module, fallback_bb->label));
            br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
            br->args[0] = alir_val_label(ctx->module, merge_bb->label);
            br->arg_count = 1;
            emit(ctx, br);

            // --- fallback_body ---
            ctx->current_block = fallback_bb;

            // Set fallback context so that `return` inside the block stores the result
            // into `res_ptr` (a stack slot) and jumps to `fallback_merge_block`.
            AlirBlock *saved_merge_block = ctx->fallback_merge_block;
            AlirValue *saved_result_slot = ctx->fallback_result_slot;
            ctx->fallback_merge_block = merge_bb;
            ctx->fallback_result_slot = res_ptr;

            ASTNode *s = bn->right;
            while (s) {
                alir_gen_stmt(ctx, s);
                s = s->next;
            }

            // Restore fallback context
            ctx->fallback_merge_block = saved_merge_block;
            ctx->fallback_result_slot = saved_result_slot;

            // If the block didn't terminate (e.g., no return), jump to merge
            if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
                emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL,
                                  alir_val_label(ctx->module, merge_bb->label), NULL));
            }

            // --- merge ---
            ctx->current_block = merge_bb;

            // Load the final result from the slot
            AlirValue *res = new_temp(ctx, res_ty);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, res, res_ptr, NULL));
            return res;
        }

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

/**
 * @brief Generate IR for a unary operation expression.
 * @param ctx The ALIR context.
 * @param un The unary operation AST node.
 * @return Result value, or NULL on failure.
 */
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

