#include "alir.h"

int alir_is_integer_type(VarType t) {
    return t.base == TYPE_INT || t.base == TYPE_CHAR || t.base == TYPE_BOOL;
}

// THIS IS ALIR STMT VAR DECLARATION
void alir_stmt_vardecl(AlirCtx *ctx, ASTNode *node) {
    VarDeclNode *vn = (VarDeclNode*)node;

    int is_array_lit = (vn->initializer && vn->initializer->type == NODE_ARRAY_LIT);

    if (is_array_lit) {
        // Evaluate the array lit on the heap
        ArrayLitNode *al = (ArrayLitNode*)vn->initializer;
        int count = 0;
        ASTNode *elem = al->elements;
        while(elem) { count++; elem = elem->next; }

        VarType elem_type = {TYPE_INT, 0, NULL};
        if (al->elements) {
            elem_type = sem_get_node_type(ctx->sem, al->elements);
            if (elem_type.base == TYPE_UNKNOWN || elem_type.base == TYPE_AUTO) {
                if (al->elements->type == NODE_LITERAL) {
                    elem_type = ((LiteralNode*)al->elements)->var_type;
                }
            }
        }

        if (vn->var_type.base == TYPE_CLASS && vn->var_type.ptr_depth == 0) {
            vn->var_type.ptr_depth = 1;
        }

        // Always decay array declarations to pointer if initialized with literal
        if (vn->var_type.base == TYPE_AUTO || vn->var_type.base == TYPE_UNKNOWN) {
            vn->var_type = elem_type;
            vn->var_type.ptr_depth++;
            vn->var_type.array_size = 0;
        } else if (vn->var_type.array_size > 0) {
            vn->var_type.array_size = 0;
            vn->var_type.ptr_depth++;
        }

        int byte_size = count > 0 ? count * alir_get_type_size(elem_type) : 8;
        AlirValue *size_val = alir_const_int(ctx->module, byte_size);

        // 1. Allocate on the Stack correctly for array sizes natively
        AlirValue *raw_mem = new_temp(ctx, (VarType){TYPE_CHAR, 1, NULL});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, raw_mem, size_val, NULL));

        VarType ptr_type = elem_type;
        if (ptr_type.array_size > 0) {
            ptr_type.ptr_depth += 2; // Array of pointers
        } else {
            ptr_type.ptr_depth++;
        }
        if (ptr_type.array_size > 0 && ptr_type.array_depth == 0) {
            // Keep array_size as is to represent pointer to array
        } else {
            ptr_type.array_size = 0;
        }
        AlirValue *heap_ptr = new_temp(ctx, ptr_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_BITCAST, heap_ptr, raw_mem, NULL));

        // 2. Loop and store
        elem = al->elements;
        int idx = 0;
        while(elem) {
            AlirValue *eval = alir_gen_expr(ctx, elem);
            if (!eval) eval = alir_const_int(ctx->module, 0);

            AlirValue *elem_ptr = new_temp(ctx, ptr_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, elem_ptr, heap_ptr, alir_const_int(ctx->module, idx)));
            emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, eval, elem_ptr));

            elem = elem->next;
            idx++;
        }

        // Declare stack pointer and save heap address
        AlirValue *var_ptr = new_temp(ctx, vn->var_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, var_ptr, NULL, NULL));
        alir_add_symbol(ctx, vn->name, var_ptr, vn->var_type);

        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, heap_ptr, var_ptr));
        return;
    }

    // Allow struct types to remain ptr_depth = 0 so they can be allocated on stack

    AlirValue *val = NULL;
    int is_stack_ctor = 0;
    VarType actual_type = vn->var_type;
    if (ctx->sem) {
        VarType inferred = sem_get_node_type(ctx->sem, (ASTNode*)vn);
        if (inferred.base != TYPE_UNKNOWN && (actual_type.base == TYPE_AUTO || actual_type.base == TYPE_UNKNOWN)) {
            actual_type.base = inferred.base;
            actual_type.class_name = inferred.class_name;
            // keep ptr_depth from vn->var_type since that reflects 'let' or explicitly written types
        }
    }

    if (vn->initializer && vn->initializer->type == NODE_CALL) {
        CallNode *cn = (CallNode*)vn->initializer;
        if (actual_type.base == TYPE_CLASS && vn->var_type.ptr_depth == 0) {
            if (alir_find_struct(ctx->module, cn->name) && actual_type.class_name && strcmp(actual_type.class_name, cn->name) == 0) {
                int arg_count = 0; ASTNode *a = cn->args; while(a) { arg_count++; a=a->next; }
                if (arg_count == 1 && ctx->sem) {
                    VarType arg_t = sem_get_node_type(ctx->sem, cn->args);
                    if (arg_t.base == TYPE_CLASS && arg_t.class_name && strcmp(arg_t.class_name, cn->name) == 0) {
                        is_stack_ctor = 2; // Copy constructor
                    } else { is_stack_ctor = 1; }
                } else {
                    is_stack_ctor = 1;
                }
            }
        }
    }

    if (is_stack_ctor == 2) {
        // Copy constructor
        AlirValue *ptr = new_temp(ctx, actual_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
        alir_add_symbol(ctx, vn->name, ptr, actual_type);
        CallNode *cn = (CallNode*)vn->initializer;
        AlirValue *arg_val = alir_gen_expr(ctx, cn->args);
        AlirValue *loaded = new_temp(ctx, actual_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, loaded, arg_val, NULL));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, loaded, ptr));
        return;
    } else if (is_stack_ctor == 1) {
        // Allocate on stack
        AlirValue *ptr = new_temp(ctx, actual_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
        alir_add_symbol(ctx, vn->name, ptr, actual_type);

        CallNode *cn = (CallNode*)vn->initializer;
        AlirInst *call_init = mk_inst(ctx->module, ALIR_OP_CALL, NULL, alir_val_global(ctx->module, cn->name, (VarType){TYPE_VOID, 0, NULL}), NULL);

        int arg_count = 0; ASTNode *a = cn->args; while(a) { arg_count++; a=a->next; }
        call_init->arg_count = arg_count + 1;
        call_init->args = alir_alloc(ctx->module, sizeof(AlirValue*) * (arg_count + 1));

        call_init->args[0] = ptr; // THIS pointer

        int i = 1; a = cn->args;
        while(a) {
            call_init->args[i++] = alir_gen_expr(ctx, a);
            a = a->next;
        }
        emit(ctx, call_init);
        return;
    }

    if (vn->initializer) {
        val = alir_gen_expr(ctx, vn->initializer);
        if (val) {
            if (vn->var_type.base == TYPE_AUTO || vn->var_type.base == TYPE_UNKNOWN) {
                vn->var_type = val->type;
            } else if (val->type.ptr_depth > vn->var_type.ptr_depth) {
                vn->var_type = val->type;
            } else if (vn->var_type.base == TYPE_CLASS && val->type.base == TYPE_CLASS) {
                vn->var_type.ptr_depth = val->type.ptr_depth;
            }
            if (val->type.is_tainted) {
                vn->var_type.is_tainted = 1;
            }
            val = promote(ctx, val, vn->var_type);
        }
    }

    // Decay array type to pointer if it is assigned a dynamic array
    if (vn->var_type.array_size > 0 && val && val->type.array_size == 0 && val->type.ptr_depth > 0) {
        vn->var_type.array_size = 0;
        vn->var_type.ptr_depth++;
    }

    AlirValue *ptr = NULL;
    if (ctx->in_flux_resume) {
        AlirSymbol *ex = alir_find_symbol(ctx, vn->name);
        if (ex) ptr = ex->ptr;
    }
    if (!ptr) {
        ptr = new_temp(ctx, vn->var_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
        alir_add_symbol(ctx, vn->name, ptr, vn->var_type);
    }

    if (vn->initializer) {
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, val ? val : alir_const_int(ctx->module, 0), ptr));
    } else if (vn->var_type.array_size == 0 && vn->var_type.ptr_depth > 0) {
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, alir_const_int(ctx->module, 0), ptr));
    }
}

void alir_stmt_assign(AlirCtx *ctx, ASTNode *node) {
    AssignNode *an = (AssignNode*)node;
    if (an->overloaded_func_name) {
        AlirValue *lhs_ptr = NULL;
        if (an->target) lhs_ptr = alir_gen_addr(ctx, an->target);
        else if (an->name) {
            AlirSymbol *s = alir_find_symbol(ctx, an->name);
            if (s) {
                lhs_ptr = s->ptr;
            } else {
                lhs_ptr = alir_val_global(ctx->module, an->name, sem_get_node_type(ctx->sem, node));
            }
        }
        AlirValue *rhs = alir_gen_expr(ctx, an->value);

        AlirValue **args = arena_alloc(ctx->sem->compiler_ctx->arena, sizeof(AlirValue*) * 2);
        args[0] = lhs_ptr;
        args[1] = rhs;
        AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, NULL, alir_val_var(ctx->module, an->overloaded_func_name), NULL);
        call->args = args;
        call->arg_count = 2;
        emit(ctx, call);
        return;
    }
    AlirValue *val = alir_gen_expr(ctx, an->value);
    if (!val) val = alir_const_int(ctx->module, 0);

    AlirValue *ptr = NULL;
    if (an->target) {
        ptr = alir_gen_addr(ctx, an->target);
    } else if (an->name) {
        AlirSymbol *s = alir_find_symbol(ctx, an->name);
        if (s) {
            ptr = s->ptr;
        } else if (an->is_implicit_let) {
            ptr = new_temp(ctx, val->type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));

            s = arena_alloc(ctx->sem->compiler_ctx->arena, sizeof(AlirSymbol));
            s->name = an->name;
            s->ptr = ptr;
            s->next = ctx->symbols;
            ctx->symbols = s;
        } else {
            ptr = alir_val_global(ctx->module, an->name, val->type);
        }
    }

    if (!ptr) {
        ptr = new_temp(ctx, val->type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
    }
    if (ptr) {
        VarType target_type = ptr->type;
        if (target_type.ptr_depth > 0) target_type.ptr_depth--;
        val = promote(ctx, val, target_type);

        // removed printf
        if (an->op != TOKEN_ASSIGN) {
            AlirValue *old_val = new_temp(ctx, target_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, old_val, ptr, NULL));

            AlirOpcode bin_op = ALIR_OP_ADD;
            switch (an->op) {
                case TOKEN_PLUS_ASSIGN: bin_op = ALIR_OP_ADD; break;
                case TOKEN_MINUS_ASSIGN: bin_op = ALIR_OP_SUB; break;
                case TOKEN_STAR_ASSIGN: bin_op = ALIR_OP_MUL; break;
                case TOKEN_SLASH_ASSIGN: bin_op = ALIR_OP_DIV; break;
                case TOKEN_MOD_ASSIGN: bin_op = ALIR_OP_MOD; break;
                case TOKEN_AND_ASSIGN: bin_op = ALIR_OP_AND; break;
                case TOKEN_OR_ASSIGN: bin_op = ALIR_OP_OR; break;
                case TOKEN_XOR_ASSIGN: bin_op = ALIR_OP_XOR; break;
                case TOKEN_LSHIFT_ASSIGN: bin_op = ALIR_OP_SHL; break;
                case TOKEN_RSHIFT_ASSIGN: bin_op = ALIR_OP_SHR; break;
                default: break;
            }

            AlirValue *new_val = new_temp(ctx, target_type);
            emit(ctx, mk_inst(ctx->module, bin_op, new_val, old_val, val));
            val = new_val;
        }
    }
    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, val, ptr));
}

void alir_stmt_while(AlirCtx *ctx, ASTNode *node) {
    WhileNode *wn = (WhileNode*)node;
    AlirBlock *cond_bb = alir_add_block(ctx->module, ctx->current_func, "while_cond");
    AlirBlock *body_bb = alir_add_block(ctx->module, ctx->current_func, "while_body");
    AlirBlock *end_bb = alir_add_block(ctx->module, ctx->current_func, "while_end");

    if (!wn->is_do_while) {
        emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));
    }

    ctx->current_block = cond_bb;
    AlirValue *cond = alir_gen_expr(ctx, wn->condition);
    if (!cond) cond = alir_const_int(ctx->module, 0);

    AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, cond, alir_val_label(ctx->module, body_bb->label));
    br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
    br->args[0] = alir_val_label(ctx->module, end_bb->label);
    br->arg_count = 1;
    emit(ctx, br);

    ctx->current_block = body_bb;
    push_loop(ctx, cond_bb, end_bb);
    ASTNode *s = wn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
    pop_loop(ctx);

    if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
        emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));
    }

    ctx->current_block = end_bb;
}

void alir_for_in_int(AlirCtx *ctx, ASTNode  *node, AlirValue *col) {
    ForInNode *fn = (ForInNode*)node;
    AlirValue *limit = col;
    AlirBlock *cond_bb = alir_add_block(ctx->module, ctx->current_func, "for_cond");
    AlirBlock *body_bb = alir_add_block(ctx->module, ctx->current_func, "for_body");
    AlirBlock *end_bb = alir_add_block(ctx->module, ctx->current_func, "for_end");

    AlirValue *var_ptr = new_temp(ctx, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, var_ptr, NULL, NULL));
    alir_add_symbol(ctx, fn->var_name, var_ptr, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, alir_const_int(ctx->module, 0), var_ptr));

    emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));
    ctx->current_block = cond_bb;

    // i < limit
    AlirValue *i_val = new_temp(ctx, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, i_val, var_ptr, NULL));
    AlirValue *valid = new_temp(ctx, (VarType){TYPE_BOOL});
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LT, valid, i_val, limit));

    AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, valid, alir_val_label(ctx->module, body_bb->label));
    br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
    br->args[0] = alir_val_label(ctx->module, end_bb->label);
    br->arg_count = 1;
    emit(ctx, br);

    ctx->current_block = body_bb;
    push_loop(ctx, cond_bb, end_bb);

    ASTNode *s = fn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }

    if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
        AlirValue *i_curr = new_temp(ctx, fn->iter_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, i_curr, var_ptr, NULL));
        AlirValue *i_next = new_temp(ctx, fn->iter_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ADD, i_next, i_curr, alir_const_int(ctx->module, 1)));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, i_next, var_ptr));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));
    }
    pop_loop(ctx);
    ctx->current_block = end_bb;
    return;
}


void alir_for_in_onstack(AlirCtx *ctx, ASTNode *node, AlirValue *col, AlirValue *limit) {
    ForInNode *fn = (ForInNode*)node;
    AlirBlock *cond_bb = alir_add_block(ctx->module, ctx->current_func, "for_cond");
    AlirBlock *body_bb = alir_add_block(ctx->module, ctx->current_func, "for_body");
    AlirBlock *end_bb  = alir_add_block(ctx->module, ctx->current_func, "for_end");

    // Setup Index Variable (idx = 0)
    AlirValue *idx_var = new_temp(ctx, (VarType){TYPE_INT, 0});
    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, idx_var, NULL, NULL));
    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, alir_const_int(ctx->module, 0), idx_var));

    // Setup Loop Element Variable (e.g. 'i')
    AlirValue *val_var = new_temp(ctx, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, val_var, NULL, NULL));
    alir_add_symbol(ctx, fn->var_name, val_var, fn->iter_type);

    emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));

    // --- COND BLOCK (idx < limit) ---
    ctx->current_block = cond_bb;
    AlirValue *curr_idx = new_temp(ctx, (VarType){TYPE_INT, 0});
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, curr_idx, idx_var, NULL));

    AlirValue *valid = new_temp(ctx, (VarType){TYPE_BOOL});
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LT, valid, curr_idx, limit));

    AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, valid, alir_val_label(ctx->module, body_bb->label));
    br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
    br->args[0] = alir_val_label(ctx->module, end_bb->label);
    br->arg_count = 1;
    emit(ctx, br);

    // --- BODY BLOCK ---
    ctx->current_block = body_bb;
    push_loop(ctx, cond_bb, end_bb);

    // 1. col is already the stack pointer to the array [N x i32]*. Use GEP natively
    AlirValue *elem_addr = new_temp(ctx, fn->iter_type);
    elem_addr->type.ptr_depth++;
    emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, elem_addr, col, curr_idx));

    // 2. Load the actual value at that array index
    AlirValue *elem_val = new_temp(ctx, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, elem_val, elem_addr, NULL));

    // 3. Store the array data into your loop variable
    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, elem_val, val_var));

    // Generate User statements
    ASTNode *s = fn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }

    // Increment idx
    if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
        AlirValue *next_idx = new_temp(ctx, (VarType){TYPE_INT, 0});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ADD, next_idx, curr_idx, alir_const_int(ctx->module, 1)));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, next_idx, idx_var));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));
    }

    pop_loop(ctx);
    ctx->current_block = end_bb;
    return;
}

void alir_for_in_ptr(AlirCtx *ctx, ASTNode *node, AlirValue *col, AlirValue *limit) {
    ForInNode *fn = (ForInNode*)node;
    AlirBlock *cond_bb = alir_add_block(ctx->module, ctx->current_func, "for_cond");
    AlirBlock *body_bb = alir_add_block(ctx->module, ctx->current_func, "for_body");
    AlirBlock *end_bb  = alir_add_block(ctx->module, ctx->current_func, "for_end");

    // Setup Index Variable (idx = 0)
    AlirValue *idx_var = new_temp(ctx, (VarType){TYPE_INT, 0});
    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, idx_var, NULL, NULL));
    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, alir_const_int(ctx->module, 0), idx_var));

    // Setup Loop Element Variable (e.g. 'i')
    AlirValue *val_var = new_temp(ctx, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, val_var, NULL, NULL));
    alir_add_symbol(ctx, fn->var_name, val_var, fn->iter_type);

    emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));

    // --- COND BLOCK (idx < limit) ---
    ctx->current_block = cond_bb;
    AlirValue *curr_idx = new_temp(ctx, (VarType){TYPE_INT, 0});
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, curr_idx, idx_var, NULL));

    AlirValue *valid = new_temp(ctx, (VarType){TYPE_BOOL});
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LT, valid, curr_idx, limit));

    AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, valid, alir_val_label(ctx->module, body_bb->label));
    br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
    br->args[0] = alir_val_label(ctx->module, end_bb->label);
    br->arg_count = 1;
    emit(ctx, br);

    // --- BODY BLOCK ---
    ctx->current_block = body_bb;
    push_loop(ctx, cond_bb, end_bb);

    // 1. Read the heap address directly out of 'col'
    AlirValue *heap_ptr = new_temp(ctx, col->type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, heap_ptr, col, NULL));

    // 2. Get the address of arr[idx] using the heap pointer
    AlirValue *elem_addr = new_temp(ctx, fn->iter_type);
    elem_addr->type.ptr_depth++;
    emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, elem_addr, heap_ptr, curr_idx));

    // 3. Load the actual value at that array index
    AlirValue *elem_val = new_temp(ctx, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, elem_val, elem_addr, NULL));

    // 4. Store the array data into your loop variable
    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, elem_val, val_var));

    // Generate User statements
    ASTNode *s = fn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }

    // Increment idx
    if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
        AlirValue *next_idx = new_temp(ctx, (VarType){TYPE_INT, 0});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ADD, next_idx, curr_idx, alir_const_int(ctx->module, 1)));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, next_idx, idx_var));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));
    }

    pop_loop(ctx);
    ctx->current_block = end_bb;
    return;
}

void alir_for_in_flux(AlirCtx *ctx, ASTNode *node, AlirValue *col) {
    ForInNode *fn = (ForInNode*)node;
    char *flux_func_name = col->type.class_name + 8;
    char resume_name[256]; snprintf(resume_name, 256, "%s_Resume", flux_func_name);

    AlirBlock *cond_bb = alir_add_block(ctx->module, ctx->current_func, "for_cond");
    AlirBlock *body_bb = alir_add_block(ctx->module, ctx->current_func, "for_body");
    AlirBlock *end_bb = alir_add_block(ctx->module, ctx->current_func, "for_end");

    emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));
    ctx->current_block = cond_bb;

    // Call Resume
    AlirValue *resume_func = alir_val_global(ctx->module, resume_name, (VarType){TYPE_VOID});
    AlirInst *call_resume = mk_inst(ctx->module, ALIR_OP_CALL, NULL, resume_func, NULL);
    call_resume->args = alir_alloc(ctx->module, sizeof(AlirValue*));
    call_resume->args[0] = col;
    call_resume->arg_count = 1;
    emit(ctx, call_resume);

    // Check finished (bool at index 1)
    AlirValue *fin_ptr = new_temp(ctx, (VarType){TYPE_BOOL, 1});
    emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, fin_ptr, col, alir_const_int(ctx->module, 1)));
    AlirValue *is_fin = new_temp(ctx, (VarType){TYPE_BOOL});
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, is_fin, fin_ptr, NULL));

    // valid = (finished == false)
    AlirValue *valid = new_temp(ctx, (VarType){TYPE_BOOL});
    AlirValue *false_val = alir_const_int(ctx->module, 0); false_val->type.base = TYPE_BOOL; emit(ctx, mk_inst(ctx->module, ALIR_OP_EQ, valid, is_fin, false_val));

    AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, valid, alir_val_label(ctx->module, body_bb->label));
    br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
    br->args[0] = alir_val_label(ctx->module, end_bb->label);
    br->arg_count = 1;
    emit(ctx, br);

    ctx->current_block = body_bb;
    push_loop(ctx, cond_bb, end_bb);

    // Load result (index 2)
    VarType res_t = fn->iter_type; res_t.ptr_depth++;
    AlirValue *res_ptr = new_temp(ctx, res_t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, res_ptr, col, alir_const_int(ctx->module, 2)));
    AlirValue *val = new_temp(ctx, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, res_ptr, NULL));

    // Store to loop var
    AlirValue *var_ptr = new_temp(ctx, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, var_ptr, NULL, NULL));
    alir_add_symbol(ctx, fn->var_name, var_ptr, fn->iter_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, val, var_ptr));

    ASTNode *s = fn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }

    if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
        emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, cond_bb->label), NULL));
    }
    pop_loop(ctx);
    ctx->current_block = end_bb;
    return;
}

// TODO split this
void alir_stmt_for_in(AlirCtx *ctx, ASTNode *node) {
    ForInNode *fn = (ForInNode*)node;
    // 1. Evaluate the collection
    AlirValue *col = NULL;
    if (fn->collection->type == NODE_VAR_REF) {
        col = alir_gen_addr(ctx, fn->collection);
        if (!col) col = alir_gen_expr(ctx, fn->collection);
    } else {
        col = alir_gen_expr(ctx, fn->collection);
    }

    if (!col) {
        printf("No collections\n");
    }

    int limit_val = 0;
    VarType col_t = sem_get_node_type(ctx->sem, fn->collection);
    if (col_t.array_size > 0) {
        limit_val = col_t.array_size;
    } else if (col->type.array_size > 0) {
        limit_val = col->type.array_size;
    } else {
        limit_val = 3; // Fallback
    }

    AlirValue *limit = alir_const_int(ctx->module, limit_val);

    if (col && col->type.base == TYPE_CLASS && col->type.class_name && strncmp(col->type.class_name, "FluxCtx_", 8) == 0) {
        printf("DEBUG: FluxCtx ptr_depth = %d\n", col->type.ptr_depth);
        if (col->type.ptr_depth == 0) {
            VarType pt = col->type;
            pt.ptr_depth = 1;
            AlirValue *size_val = new_temp(ctx, (VarType){TYPE_INT});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_SIZEOF, size_val, alir_val_type(ctx->module, col->type.class_name), NULL));
            AlirValue *raw_mem = new_temp(ctx, (VarType){TYPE_CHAR, 1});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, raw_mem, size_val, NULL));
            AlirValue *col_ptr = new_temp(ctx, pt);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_BITCAST, col_ptr, raw_mem, NULL));
            emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, col, col_ptr));
            col = col_ptr;
        }
        return alir_for_in_flux(ctx, node, col);
    }

    if (col && alir_is_integer_type(col->type) && (col->type.ptr_depth + col->type.array_size) == 0) {
        return alir_for_in_int(ctx, node, col);
    }

    if (col && (col->type.ptr_depth > 0)) {
        return alir_for_in_ptr(ctx, node, col, limit);
    }

    if (col && (col->type.array_size > 0)) {
        return alir_for_in_onstack(ctx, node, col, limit);
    }

    // If it makes it here, the collection evaluation completely failed!
    printf("COMPILER ERROR: Attempted to iterate over an invalid or null collection!\n");
    return;
}
