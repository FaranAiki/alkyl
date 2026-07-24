#include "alir.h"

// --- NEW HELPER: Dynamically calculate byte sizes for allocations ---
int alir_get_type_size(VarType t) {
    // All pointers are 8 bytes on a 64-bit architecture
    if (t.ptr_depth > 0) return 8; 
    
    switch (t.base) {
        case TYPE_VOID: return 0;
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_UNSIGNED_CHAR: return 1;
        case TYPE_INT:
        case TYPE_UNSIGNED_INT:
        case TYPE_SINGLE: return 4;
        case TYPE_LONG:
        case TYPE_DOUBLE: return 8;
        // TODO: Struct/Class size calculation goes here later
        default: return 8; // Safe fallback
    }
}

int alir_robust_get_field_index(AlirCtx *ctx, const char *hint_class, const char *field_name) {
    int idx = -1;
    if (hint_class) {
        idx = alir_get_field_index(ctx->module, hint_class, field_name);
    }
    if (idx == -1) {
        AlirStruct *search = ctx->module->structs;
        while (search) {
            AlirField *f = search->fields;
            while(f) {
                if (strcmp(f->name, field_name) == 0) return f->index;
                f = f->next;
            }
            search = search->next;
        }
    }
    return idx == -1 ? 0 : idx;
}

AlirValue* alir_gen_array_lit(AlirCtx *ctx, ASTNode *node);

// Handles L-Values: Returns the memory address of arr[index]
AlirValue* alir_gen_addr_index_access(AlirCtx *ctx, IndexAccessNode *aa) {
    // 1. Get the address of the array variable itself
    AlirValue *base_ptr = alir_gen_addr(ctx, aa->target);
    if (!base_ptr) return NULL;

    VarType alir_type = base_ptr->type;

    // 2. CRITICAL: If target is a dynamic pointer (like from malloc), 
    // we must load the heap address FROM the stack variable before indexing!
    // We look at the actual ALIR variable type to determine if it's decayed
    // to a pointer instead of static inline representation on the stack.
    if (alir_type.ptr_depth > 0 && alir_type.array_size == 0) {
        AlirValue *loaded_base = new_temp(ctx, alir_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, loaded_base, base_ptr, NULL));
        base_ptr = loaded_base; // Now base_ptr holds the malloc result
    }

    // 3. Evaluate the index
    AlirValue *index = NULL;
    VarType elem_t = sem_get_node_type(ctx->sem, (ASTNode*)aa);
    VarType target_t = sem_get_node_type(ctx->sem, aa->target);
    
    // Check if it's a trait access: trait access is represented as ArrayAccess but the result type 
    // is a different class than the target type
    if (elem_t.base == TYPE_CLASS && target_t.base == TYPE_CLASS && 
        elem_t.class_name && target_t.class_name &&
        strcmp(elem_t.class_name, target_t.class_name) != 0) {
        
        VarType ptr_t = elem_t;
        ptr_t.ptr_depth++;
        
        AlirValue *elem_ptr = new_temp(ctx, ptr_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_BITCAST, elem_ptr, base_ptr, NULL));
        return elem_ptr;
    }
    
    index = alir_gen_expr(ctx, aa->index);
    if (!index) index = alir_const_int(ctx->module, 0); 
    
    // 4. Calculate the type of the pointer we are creating
    elem_t = sem_get_node_type(ctx->sem, (ASTNode*)aa);
    VarType ptr_t = elem_t;
    ptr_t.ptr_depth++; 

    // 5. Emit GET_PTR (LLVM getelementptr) and RETURN THE POINTER
    AlirValue *elem_ptr = new_temp(ctx, ptr_t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, elem_ptr, base_ptr, index));
    
    return elem_ptr; 
}

// Handles R-Values: Returns the actual data inside arr[index]
AlirValue* alir_gen_expr_index_access(AlirCtx *ctx, IndexAccessNode *aa) {
    VarType elem_t = sem_get_node_type(ctx->sem, (ASTNode*)aa);
    VarType target_t = sem_get_node_type(ctx->sem, aa->target);
    
    // Check if it's a trait access. If so, just return the target bitcasted!
    if (elem_t.base == TYPE_CLASS && target_t.base == TYPE_CLASS && 
        elem_t.class_name && target_t.class_name &&
        strcmp(elem_t.class_name, target_t.class_name) != 0) {
        
        AlirValue *target_val = alir_gen_expr(ctx, aa->target);
        if (!target_val) return NULL;
        
        AlirValue *casted = new_temp(ctx, elem_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_BITCAST, casted, target_val, NULL));
        return casted;
    }

    // 1. Get the memory address of the element
    AlirValue *elem_ptr = alir_gen_addr_index_access(ctx, aa);
    if (!elem_ptr) return NULL;

    // 2. Emit a LOAD instruction to read the actual value
    AlirValue *loaded_val = new_temp(ctx, elem_t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, loaded_val, elem_ptr, NULL));
    return loaded_val;
}

AlirValue* alir_gen_addr(AlirCtx *ctx, ASTNode *node) {
    if (!node) return NULL;

    if (node->type == NODE_VAR_REF) {
        return alir_gen_addr_var_ref(ctx, node);
    }
    
    if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccessNode *ma = (MemberAccessNode*)node;
        VarType obj_t = sem_get_node_type(ctx->sem, ma->object);
        if (obj_t.base == TYPE_UNKNOWN && ma->object->type == NODE_VAR_REF) {
            AlirSymbol *sym = alir_find_symbol(ctx, ((VarRefNode*)ma->object)->name);
            if (sym) obj_t = sym->type;
        }
        if (obj_t.base == TYPE_ENUM) return NULL; 

        AlirValue *base_ptr = NULL;
        if (obj_t.ptr_depth == 0) {
            base_ptr = alir_gen_addr(ctx, ma->object);
            if (!base_ptr) {
                AlirValue *rval = alir_gen_expr(ctx, ma->object);
                if (rval) {
                    base_ptr = new_temp(ctx, obj_t);
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, base_ptr, NULL, NULL));
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, rval, base_ptr));
                }
            }
        } else {
            base_ptr = alir_gen_expr(ctx, ma->object);
        }
        
        if (!base_ptr) return NULL;

        char *class_name = base_ptr->type.class_name;
        if (!class_name && obj_t.class_name) class_name = obj_t.class_name;
        if (!class_name && ma->object->type == NODE_VAR_REF) {
            AlirSymbol *sym = alir_find_symbol(ctx, ((VarRefNode*)ma->object)->name);
            if (sym && sym->type.class_name) class_name = sym->type.class_name;
        }

        int idx = alir_robust_get_field_index(ctx, class_name, ma->member_name);
        
        // Find field type for precise IR typing
        VarType field_type = {TYPE_AUTO, 0, NULL};
        if (class_name) {
            AlirStruct *st = alir_find_struct(ctx->module, class_name);
            if (st) {
                AlirField *f = st->fields;
                while(f) {
                    if (strcmp(f->name, ma->member_name) == 0) { field_type = f->type; break; }
                    f = f->next;
                }
            }
        }
        
        if (field_type.base == TYPE_AUTO) {
            AlirStruct *search = ctx->module->structs;
            while (search) {
                AlirField *f = search->fields;
                while(f) {
                    if (strcmp(f->name, ma->member_name) == 0) { field_type = f->type; break; }
                    f = f->next;
                }
                if (field_type.base != TYPE_AUTO) break;
                search = search->next;
            }
        }
        
        field_type.ptr_depth++; // Yields a pointer to the field
        AlirValue *res = new_temp(ctx, field_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, res, base_ptr, alir_const_int(ctx->module, idx)));
        return res;
    }
    
    // no need to change
    if (node->type == NODE_INDEX_ACCESS) {
        return alir_gen_addr_index_access(ctx, (IndexAccessNode*)node);
    }
    
    if (node->type == NODE_ARRAY_LIT) {
        return alir_gen_array_lit(ctx, node);
    }

    // TODO add vector access
    if (node->type == NODE_VECTOR_ACCESS) {
        
    }  

    return NULL;
}



// TODO add this for literal
AlirValue* alir_gen_literal(AlirCtx *ctx, LiteralNode *ln) {
    if (ln->var_type.ptr_depth == 0 && ln->var_type.array_size == 0) {
        switch (ln->var_type.base) {
            case TYPE_INT:
                return alir_const_int(ctx->module, ln->val.int_val);
            case TYPE_LONG:
                return alir_const_long(ctx->module, ln->val.long_val); 
            case TYPE_SINGLE:
                return alir_const_float(ctx->module, ln->val.single_val);
            case TYPE_DOUBLE:
                return alir_const_double(ctx->module, ln->val.double_val);
            case TYPE_UNSIGNED_INT:
                return alir_const_unsigned_int(ctx->module, ln->val.unsigned_int_val);
            case TYPE_CHAR:
                return alir_const_char(ctx->module, ln->val.char_val);
            case TYPE_UNSIGNED_CHAR:
                return alir_const_unsigned_char(ctx->module, ln->val.unsigned_char_val);
            default: break; // TODO here
        }
    }
    
    if ((ln->var_type.base == TYPE_CLASS && ln->var_type.class_name && strcmp(ln->var_type.class_name, "string") == 0) || (ln->var_type.base == TYPE_CHAR && ln->var_type.ptr_depth > 0)) {
        if (!ln->val.str_val || (long)ln->val.str_val <= 0x1000) {
            return alir_const_int(ctx->module, ln->val.long_val);
        }
        
        AlirValue *glob = alir_module_add_string_literal(ctx->module, ln->val.str_val, ln->var_type, ctx->str_counter++);
        if (ln->var_type.base == TYPE_CLASS && strcmp(ln->var_type.class_name, "string") == 0) {
            AlirValue *val = new_temp(ctx, ln->var_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, glob, NULL));
            return val;
        }
        return glob;
    }
    
    if (ln->var_type.base == TYPE_BOOL) {
        return alir_const_int(ctx->module, ln->val.long_val);
    }
    
    if (ln->var_type.base == TYPE_UNKNOWN && ln->var_type.ptr_depth == 1) {
        AlirValue *v = alir_alloc(ctx->module, sizeof(AlirValue));
        v->kind = ALIR_VAL_CONST;
        v->type = ln->var_type;
        v->val.int_val = 0;
        return v;
    }

    // Fallback for empty/unhandled literals
    // TODO fix this
    printf("Unknown literal!\n");
    return alir_const_int(ctx->module, 0);
}

AlirValue* alir_gen_var_ref(AlirCtx *ctx, VarRefNode *vn) {
    if (vn->is_error_id) {
        return alir_const_int(ctx->module, vn->error_id);
    }

    AlirValue *ptr = alir_gen_addr(ctx, (ASTNode*)vn);
    if (!ptr) {
        // If it's a global function or global variable
        SemSymbol *sym = sem_symbol_lookup(ctx->sem, vn->name, NULL);
        if (sym && sym->kind == SYM_FUNC) {
            VarType t = sem_get_node_type(ctx->sem, (ASTNode*)vn);
            // Function pointer type needs to be treated as a pointer
            if (!t.is_func_ptr) {
                VarType ptr_type = t;
                ptr_type.is_func_ptr = 1;
                ptr_type.fp_ret_type = alir_alloc(ctx->module, sizeof(VarType));
                *ptr_type.fp_ret_type = t;
                t = ptr_type;
            }
            return alir_val_global(ctx->module, sym->mangled_name ? sym->mangled_name : vn->name, t);
        } else if (sym && sym->kind == SYM_VAR) {
            VarType t = sem_get_node_type(ctx->sem, (ASTNode*)vn);
            t.ptr_depth++; // Make it a pointer type because it's an address
            ptr = alir_val_global(ctx->module, sym->mangled_name ? sym->mangled_name : vn->name, t);
        } else {
            return NULL; // Safety guard against unresolved allocas
        }
    }

    // Get precise type from Semantics
    VarType t = sem_get_node_type(ctx->sem, (ASTNode*)vn);
    
    AlirSymbol *asym = alir_find_symbol(ctx, vn->name);
    if (!asym && vn->is_class_member) {
        // [FIX] Field Access Rewrite
        MemberAccessNode *ma = arena_alloc_type(ctx->sem->compiler_ctx->arena, MemberAccessNode);
        ma->base.type = NODE_MEMBER_ACCESS;
        ma->base.line = vn->base.line;
        ma->base.col = vn->base.col;
        VarRefNode *th = arena_alloc_type(ctx->sem->compiler_ctx->arena, VarRefNode);
        th->base.type = NODE_VAR_REF;
        th->name = arena_strdup(ctx->sem->compiler_ctx->arena, "this");
        ma->object = (ASTNode*)th;
        ma->member_name = vn->name;
        AlirValue *res = alir_gen_addr(ctx, (ASTNode*)ma);
        
        // WE MUST LOAD THE VALUE, BECAUSE alir_gen_var_ref IS SUPPOSED TO RETURN THE R-VALUE (loaded value)
        VarType t = sem_get_node_type(ctx->sem, (ASTNode*)vn);
        if (t.base == TYPE_UNKNOWN && res) {
            t = res->type;
            if (t.ptr_depth > 0) t.ptr_depth--;
        }
        if (t.array_size > 0) return res;
        AlirValue *val = new_temp(ctx, t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, res, NULL));
        return val;
    }
    if (asym && asym->ptr == ptr) {
        // Address came directly from an ALLOCA. Type is intact.
        if (asym->type.base != TYPE_UNKNOWN && asym->type.base != TYPE_AUTO) {
            t = asym->type;
        }
    } else {
        // Address came from a GET_PTR (e.g. implicit `this.` field indexing). It's a T*.
        if (ptr->type.base != TYPE_UNKNOWN && ptr->type.base != TYPE_AUTO) {
            t = ptr->type;
            if (t.ptr_depth > 0) t.ptr_depth--;
        }
    }
    
    if (t.array_size > 0) {
        return ptr;
    }
    
    AlirValue *val = new_temp(ctx, t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, ptr, NULL));
    return val;
}

AlirValue* alir_gen_access(AlirCtx *ctx, ASTNode *node) {
    // Special Enum Handling: If member access resolves to an Enum Type
    if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccessNode *ma = (MemberAccessNode*)node;
        VarType obj_t = sem_get_node_type(ctx->sem, ma->object);
        if (obj_t.base == TYPE_ENUM && obj_t.class_name) {
            long val = 0;
            if (alir_get_enum_value(ctx->module, obj_t.class_name, ma->member_name, &val)) {
                return alir_const_int(ctx->module, val);
            }
        }
    }

    AlirValue *ptr = alir_gen_addr(ctx, node);
    if (!ptr) return NULL; 
    
    VarType t = sem_get_node_type(ctx->sem, node);
    
    // [FIX] ALWAYS trust GET_PTR's physical type over Semantic Analyzer inference bounds.
    // Address returned here is a GET_PTR so it represents a T*. We dynamically extract T.
    if (ptr->type.base != TYPE_UNKNOWN && ptr->type.base != TYPE_AUTO) {
        t = ptr->type;
        if (t.ptr_depth > 0) t.ptr_depth--;
    }
    
    AlirValue *val = new_temp(ctx, t); 
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, ptr, NULL));
    return val;
}

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
                    r_type.base == TYPE_SINGLE || r_type.base == TYPE_DOUBLE);

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
        // ... add other cases
    }
    
    // Result type logic
    if (op == ALIR_OP_EQ || op == ALIR_OP_LT || op == ALIR_OP_GT || op == ALIR_OP_LTE || op == ALIR_OP_GTE || op == ALIR_OP_NEQ) res_type = (VarType){TYPE_BOOL, 0};
    
    if (l->kind == ALIR_VAL_CONST && r->kind == ALIR_VAL_CONST) {
        if (op == ALIR_OP_EQ) {
            return alir_const_int(ctx->module, l->val.int_val == r->val.int_val ? 1 : 0);
        } else if (op == ALIR_OP_NEQ) {
            return alir_const_int(ctx->module, l->val.int_val != r->val.int_val ? 1 : 0);
        }
    }

    AlirValue *dest = new_temp(ctx, res_type);
    emit(ctx, mk_inst(ctx->module, op, dest, l, r));
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
        case TOKEN_NOT: 
            op = ALIR_OP_NOT; 
            break;
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

AlirValue* alir_gen_inc_dec(AlirCtx *ctx, IncDecNode *id) {
    if (id->overloaded_func_name) {
        AlirValue *operand = alir_gen_expr(ctx, id->target);
        AlirValue **args = arena_alloc(ctx->sem->compiler_ctx->arena, sizeof(AlirValue*));
        args[0] = operand;
        VarType res_ty = sem_get_node_type(ctx->sem, (ASTNode*)id);
        AlirValue *res = NULL;
        if (res_ty.base != TYPE_VOID || res_ty.ptr_depth > 0) {
            res = new_temp(ctx, res_ty);
        }
        AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, res, alir_val_var(ctx->module, id->overloaded_func_name), NULL);
        call->args = args;
        call->arg_count = 1;
        emit(ctx, call);
        return res;
    }

    AlirValue *ptr = alir_gen_addr(ctx, id->target);
    if (!ptr) {
        ptr = new_temp(ctx, (VarType){TYPE_INT, 1});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
    }

    VarType t = sem_get_node_type(ctx->sem, id->target);
    AlirValue *val = new_temp(ctx, t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, ptr, NULL));
    
    AlirValue *one = (t.base == TYPE_SINGLE || t.base == TYPE_DOUBLE) ? 
        alir_const_float(ctx->module, 1.0) : alir_const_int(ctx->module, 1);
        
    AlirOpcode op = (id->op == TOKEN_INCREMENT) ? ALIR_OP_ADD : ALIR_OP_SUB;
    if (t.base == TYPE_SINGLE || t.base == TYPE_DOUBLE) {
        op = (id->op == TOKEN_INCREMENT) ? ALIR_OP_FADD : ALIR_OP_FSUB;
    }
    
    AlirValue *new_val = new_temp(ctx, t);
    emit(ctx, mk_inst(ctx->module, op, new_val, val, one));
   
    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, new_val, ptr));
    
    if (id->is_prefix) return new_val;
    return val;
}

AlirValue* alir_gen_cast(AlirCtx *ctx, CastNode *cn) {
    if (cn->custom_cast_method) {
        VarType obj_t = sem_get_node_type(ctx->sem, cn->operand);
        if (obj_t.base == TYPE_UNKNOWN && cn->operand->type == NODE_VAR_REF) {
            AlirSymbol *sym = alir_find_symbol(ctx, ((VarRefNode*)cn->operand)->name);
            if (sym) obj_t = sym->type;
        }
        AlirValue *this_val = NULL;
        if (obj_t.ptr_depth == 0) {
            this_val = alir_gen_addr(ctx, cn->operand);
            if (!this_val) {
                AlirValue *rval = alir_gen_expr(ctx, cn->operand);
                if (rval) {
                    this_val = new_temp(ctx, obj_t);
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, this_val, NULL, NULL));
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, rval, this_val));
                }
            }
        } else {
            this_val = alir_gen_expr(ctx, cn->operand); 
        }
        
        AlirValue *func_val = alir_val_var(ctx->module, cn->custom_cast_method);
        AlirValue *dest = new_temp(ctx, cn->var_type);
        AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, dest, func_val, NULL);
        call->arg_count = 1;
        call->args = alir_alloc(ctx->module, sizeof(AlirValue*));
        call->args[0] = this_val;
        emit(ctx, call);
        return dest;
    }

    AlirValue *operand = alir_gen_expr(ctx, cn->operand);
    if (!operand) {
        operand = new_temp(ctx, (VarType){TYPE_INT, 0});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, operand, NULL, NULL));
    }
    
    VarType res_type = cn->var_type;
    AlirValue *dest = new_temp(ctx, res_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_CAST, dest, operand, NULL));
    return dest;
}


AlirValue* alir_gen_call_std(AlirCtx *ctx, CallNode *cn) {
    const char *target_name = cn->mangled_name ? cn->mangled_name : cn->name;
    if (!target_name && cn->target) {
        if (cn->target->type == NODE_VAR_REF) {
            target_name = ((VarRefNode*)cn->target)->name;
        } else if (cn->target->type == NODE_TEMPLATE_INSTANTIATION) {
            TemplateInstNode *ti = (TemplateInstNode*)cn->target;
            if (ti->target && ti->target->type == NODE_VAR_REF) {
                target_name = ((VarRefNode*)ti->target)->name;
            }
        }
    }

    if (cn->target && (cn->target->type == NODE_MEMBER_ACCESS || cn->target->type == NODE_VAR_REF) && ctx->sem && cn->name) {
        ASTNode *object_node = NULL;
        VarType obj_t = {TYPE_UNKNOWN, 0};
        
        if (cn->target->type == NODE_MEMBER_ACCESS) {
            MemberAccessNode *ma = (MemberAccessNode*)cn->target;
            object_node = ma->object;
            obj_t = sem_get_node_type(ctx->sem, ma->object);
        } else if (cn->target->type == NODE_VAR_REF) {
            VarRefNode *vn = (VarRefNode*)cn->target;
            if (vn->is_class_member) {
                AlirSymbol *this_sym = alir_find_symbol(ctx, "this");
                if (this_sym && this_sym->type.base == TYPE_CLASS) {
                    obj_t = this_sym->type;
                    
                    // Create a fake object node for 'this'
                    VarRefNode *fake_this = alir_alloc(ctx->module, sizeof(VarRefNode));
                    fake_this->base.type = NODE_VAR_REF;
                    fake_this->name = "this";
                    object_node = (ASTNode*)fake_this;
                }
            }
        }
        
        if (object_node && obj_t.base == TYPE_CLASS && obj_t.class_name) {
            SemSymbol *sym = NULL;
            SemSymbol *class_sym = sem_symbol_lookup(ctx->sem, obj_t.class_name, NULL);
            if (class_sym && class_sym->inner_scope) {
                SemSymbol *s = class_sym->inner_scope->symbols;
                while (s) {
                    if (strcmp(s->name, cn->name) == 0) {
                        sym = s;
                        break;
                    }
                    s = s->next;
                }
            }
            
            if (!sym) {
                sym = sem_symbol_lookup(ctx->sem, cn->name, NULL);
            }
            
            if (sym && sym->kind == SYM_FUNC) {
                MethodCallNode mc;
                memset(&mc, 0, sizeof(MethodCallNode));
                mc.base.type = NODE_METHOD_CALL;
                mc.base.line = cn->base.line;
                mc.base.col = cn->base.col;
                mc.object = object_node;
                mc.method_name = cn->name;
                mc.mangled_name = cn->mangled_name;
                mc.args = cn->args;
                mc.owner_class = obj_t.class_name;
                return alir_gen_method_call(ctx, &mc);
            }
        }
    }

    AlirValue *func_val = NULL;
    if (cn->target && cn->target->type != NODE_TEMPLATE_INSTANTIATION) {
        func_val = alir_gen_expr(ctx, cn->target);
    }
    if (!func_val) {
        func_val = alir_val_var(ctx->module, target_name);
    }
    AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, NULL, func_val, NULL);
    
    int count = 0; ASTNode *a = cn->args; while(a) { count++; a=a->next; }
    if (count == 1 && ctx->sem && target_name) {
        SemSymbol *sym = sem_symbol_lookup(ctx->sem, target_name, NULL);
        if (sym && sym->kind == SYM_CLASS) {
            VarType arg_t = sem_get_node_type(ctx->sem, cn->args);
            if (arg_t.base == TYPE_CLASS && arg_t.class_name && strcmp(arg_t.class_name, target_name) == 0) {
                AlirValue *arg_val = alir_gen_expr(ctx, cn->args);
                AlirValue *ptr = new_temp(ctx, arg_t);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
                AlirValue *loaded = new_temp(ctx, arg_t);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, loaded, arg_val, NULL));
                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, loaded, ptr));
                return ptr;
            }
        }
    }

    call->arg_count = count;
    call->args = alir_alloc(ctx->module, sizeof(AlirValue*) * count);
    
    int i = 0; a = cn->args;
    while(a) {
        AlirValue *arg_val = alir_gen_expr(ctx, a);
        if (!arg_val) {
             arg_val = new_temp(ctx, (VarType){TYPE_INT, 0});
             emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, arg_val, NULL, NULL));
        }
        call->args[i++] = arg_val;
        a = a->next;
    }
    
    // Result type from Semantic Table
    VarType ret_type = sem_get_node_type(ctx->sem, (ASTNode*)cn);
    
    // [FIX] Infer flux generator context type properly to prevent Array-like iteration SIGSEGVs
    int found = 0;
    if (ctx->sem && target_name) {
        SemScope *scope = NULL;
        SemSymbol *sym = sem_symbol_lookup(ctx->sem, target_name, &scope);
        if (sym && sym->kind == SYM_FUNC && sym->is_flux) {
            char struct_name[512];
            snprintf(struct_name, sizeof(struct_name), "FluxCtx_%s", target_name);
            ret_type = (VarType){TYPE_CLASS, 1, alir_strdup(ctx->module, struct_name), 0, 0, NULL, NULL, 0, 0, 0, 0};
            found = 1;
        }
    }
    
    if (!found && ctx->module && target_name) {
        // Fallback if Semantic Analyzer runs dry or was cleaned up by driver
        AlirFunction *f = ctx->module->functions;
        while(f) {
            if (f->name && strcmp(f->name, target_name) == 0 && f->is_flux) {
                ret_type = f->ret_type;
                found = 1;
                break;
            }
            f = f->next;
        }
    }
    
    AlirValue *dest = new_temp(ctx, ret_type); 
    call->dest = dest;
    emit(ctx, call);
    return dest;
}

AlirValue* alir_gen_call(AlirCtx *ctx, CallNode *cn) {
    const char *target_name = cn->mangled_name ? cn->mangled_name : cn->name;
    // Check if it's a constructor call via Struct Registry
    if (alir_find_struct(ctx->module, target_name)) {
        int count = 0; ASTNode *a = cn->args; while(a) { count++; a=a->next; }
        if (count == 1 && ctx->sem) {
            VarType arg_t = sem_get_node_type(ctx->sem, cn->args);
            if (arg_t.base == TYPE_CLASS && arg_t.class_name && strcmp(arg_t.class_name, target_name) == 0) {
                return alir_gen_expr(ctx, cn->args);
            }
        }
        return alir_lower_new_object(ctx, target_name, cn->args);
    }
    
    // Intercept Macro function calls
    if (ctx->sem) {
        SemSymbol *sym = sem_symbol_lookup(ctx->sem, target_name, NULL);
        if (sym && sym->kind == SYM_FUNC && sym->is_macro && sym->node_ptr) {
            FuncDefNode *fd = (FuncDefNode*)sym->node_ptr;
            
            // Collect macro arguments and parameters
            int num_params = 0;
            Parameter *p = fd->params;
            while(p) { num_params++; p = p->next; }
            
            char **param_names = NULL;
            ASTNode **param_args = NULL;
            ASTNode *varargs_head = NULL;
            
            if (num_params > 0) {
                param_names = alir_alloc(ctx->module, num_params * sizeof(char*));
                param_args = alir_alloc(ctx->module, num_params * sizeof(ASTNode*));
                p = fd->params;
                ASTNode *a = cn->args;
                for (int i=0; i<num_params && a; i++) {
                    param_names[i] = p->name;
                    param_args[i] = a;
                    p = p->next;
                    a = a->next;
                }
                varargs_head = a; // Any remaining args are varargs
            } else {
                varargs_head = cn->args;
            }
            
            // Clone the AST body so we don't modify the original macro definition
            CompilerContext *cctx = ctx->module->compiler_ctx;
            ASTNode *cloned_body = ast_clone(cctx, fd->body, NULL, NULL, 0, NULL, NULL, 0);
            
            // Rewrite variable references and varargs inside the cloned body
            cloned_body = ast_rewrite_macro(cctx, cloned_body, varargs_head, param_names, param_args, num_params);
            
             
             
            
            // Run semantic analysis on the expanded macro body
            extern void sem_check_block(SemanticCtx *ctx, ASTNode *block);
            sem_check_block(ctx->sem, cloned_body);
            
            // Compile the rewritten AST directly into the current caller's ALIR block
            ASTNode *curr = cloned_body;
            while (curr) {
                alir_gen_stmt(ctx, curr);
                curr = curr->next;
            }
            
            return new_temp(ctx, (VarType){TYPE_VOID, 0});
        }
    }
    
    return alir_gen_call_std(ctx, cn);
}

AlirValue* alir_gen_method_call(AlirCtx *ctx, MethodCallNode *mc) {
    VarType obj_t = sem_get_node_type(ctx->sem, mc->object);
    if (obj_t.base == TYPE_UNKNOWN && mc->object->type == NODE_VAR_REF) {
        AlirSymbol *sym = alir_find_symbol(ctx, ((VarRefNode*)mc->object)->name);
        if (sym) obj_t = sym->type;
    }
    AlirValue *this_val = NULL;
    
    if (obj_t.ptr_depth == 0) {
        this_val = alir_gen_addr(ctx, mc->object);
        if (!this_val) {
            AlirValue *rval = alir_gen_expr(ctx, mc->object);
            if (rval) {
                this_val = new_temp(ctx, obj_t);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, this_val, NULL, NULL));
                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, rval, this_val));
            }
        }
    } else {
        this_val = alir_gen_expr(ctx, mc->object); 
    }
    
    if (!this_val) {
         this_val = new_temp(ctx, (VarType){TYPE_INT, 0});
         emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, this_val, NULL, NULL));
    }

    char *cname = obj_t.class_name;
    
    // [BUGFIX] Mangling Failure Recovery: Check IR types and Local Symtable dynamically
    if (!cname && this_val && this_val->type.class_name) {
        cname = this_val->type.class_name;
    }
    if (!cname && mc->object->type == NODE_VAR_REF) {
        AlirSymbol *sym = alir_find_symbol(ctx, ((VarRefNode*)mc->object)->name);
        if (sym && sym->type.class_name) cname = sym->type.class_name;
    }

    char func_name[256];
    if (mc->mangled_name) {
        snprintf(func_name, 256, "%s", mc->mangled_name);
    } else {
        if (cname) snprintf(func_name, 256, "%s_%s", cname, mc->method_name);
        else snprintf(func_name, 256, "%s", mc->method_name);
    }

    AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, NULL, alir_val_var(ctx->module, func_name), NULL);
    
    int count = 0; ASTNode *a = mc->args; while(a) { count++; a=a->next; }
    if (mc->is_static) {
        call->arg_count = count;
        call->args = alir_alloc(ctx->module, sizeof(AlirValue*) * count);
        int i = 0; a = mc->args;
        while(a) {
            AlirValue *arg_val = alir_gen_expr(ctx, a);
            if (!arg_val) {
                 arg_val = new_temp(ctx, (VarType){TYPE_INT, 0});
                 emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, arg_val, NULL, NULL));
            }
            call->args[i++] = arg_val;
            a = a->next;
        }
    } else {
        call->arg_count = count + 1;
        call->args = alir_alloc(ctx->module, sizeof(AlirValue*) * (count + 1));
        
        call->args[0] = this_val;
        int i = 1; a = mc->args;
        while(a) {
            AlirValue *arg_val = alir_gen_expr(ctx, a);
            if (!arg_val) {
                 arg_val = new_temp(ctx, (VarType){TYPE_INT, 0});
                 emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, arg_val, NULL, NULL));
            }
            call->args[i++] = arg_val;
            a = a->next;
        }
    }
    
    VarType ret_type = sem_get_node_type(ctx->sem, (ASTNode*)mc);
    
    // [FIX] Infer flux generator context type properly for methods to prevent SIGSEGVs
    int found_flux = 0;
    if (ctx->sem && cname) {
        SemScope *scope = NULL;
        SemSymbol *class_sym = sem_symbol_lookup(ctx->sem, cname, &scope);
        if (class_sym && class_sym->inner_scope) {
            SemSymbol *method_sym = class_sym->inner_scope->symbols;
            while(method_sym) {
                if (strcmp(method_sym->name, mc->method_name) == 0) {
                    if (method_sym->is_flux) {
                        char struct_name[512];
                        snprintf(struct_name, sizeof(struct_name), "FluxCtx_%s", func_name);
                        ret_type = (VarType){TYPE_CLASS, 1, alir_strdup(ctx->module, struct_name), 0, 0, NULL, NULL, 0, 0, 0, 0};
                        found_flux = 1;
                    }
                    break;
                }
                method_sym = method_sym->next;
            }
        }
    }
    
    if (!found_flux && ctx->module) {
        AlirFunction *f = ctx->module->functions;
        while(f) {
            if (strcmp(f->name, func_name) == 0 && f->is_flux) {
                ret_type = f->ret_type;
                break;
            }
            f = f->next;
        }
    }

    AlirValue *dest = new_temp(ctx, ret_type);
    call->dest = dest;
    emit(ctx, call);
    return dest;
}

// Lowers an array literal (e.g. [1, 2, 3])
AlirValue* alir_gen_array_lit(AlirCtx *ctx, ASTNode *node) {
    ArrayLitNode *al = (ArrayLitNode*)node;
    
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

    VarType arr_type = elem_type;
    arr_type.array_size = count > 0 ? count : 1; 

    VarType ptr_type = elem_type;
    ptr_type.ptr_depth++; 
    ptr_type.array_size = arr_type.array_size; 
    
    // 1. Allocate on the Stack natively
    AlirValue *stack_ptr = new_temp(ctx, arr_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, stack_ptr, NULL, NULL));
    
    // 2. Loop and store
    elem = al->elements;
    int idx = 0;
    while(elem) {
        AlirValue *eval = alir_gen_expr(ctx, elem);
        if (!eval) eval = alir_const_int(ctx->module, 0);

        AlirValue *elem_ptr = new_temp(ctx, ptr_type); 
        emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, elem_ptr, stack_ptr, alir_const_int(ctx->module, idx)));
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, eval, elem_ptr));
        
        elem = elem->next; 
        idx++;
    }

    return stack_ptr; 
}

AlirValue* alir_gen_expr(AlirCtx *ctx, ASTNode *node) {
    if (!node) return NULL;
    
    ctx->current_line = node->line;
    ctx->current_col = node->col;

    switch(node->type) {
        case NODE_ARRAY_LIT: return alir_gen_array_lit(ctx, node);
        case NODE_LITERAL: return alir_gen_literal(ctx, (LiteralNode*)node);
        case NODE_VAR_REF: return alir_gen_var_ref(ctx, (VarRefNode*)node);
        case NODE_ASSIGN: {
            AssignNode *an = (AssignNode*)node;
            if (an->overloaded_func_name) {
                AlirValue *lhs_ptr = NULL;
                if (an->target) lhs_ptr = alir_gen_addr(ctx, an->target);
                else if (an->name) {
                    AlirSymbol *s = alir_find_symbol(ctx, an->name);
                    if (s) lhs_ptr = s->ptr;
                    else lhs_ptr = alir_val_global(ctx->module, an->name, sem_get_node_type(ctx->sem, node));
                }
                AlirValue *rhs = alir_gen_expr(ctx, an->value);
                AlirValue **args = arena_alloc(ctx->sem->compiler_ctx->arena, sizeof(AlirValue*) * 2);
                args[0] = lhs_ptr; args[1] = rhs;
                AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, NULL, alir_val_var(ctx->module, an->overloaded_func_name), NULL);
                call->args = args; call->arg_count = 2;
                emit(ctx, call);
                return rhs;
            }
            AlirValue *val = alir_gen_expr(ctx, an->value);
            if (!val) val = alir_const_int(ctx->module, 0);
            AlirValue *ptr = NULL;
            if (an->target) ptr = alir_gen_addr(ctx, an->target);
            else if (an->name) {
                AlirSymbol *s = alir_find_symbol(ctx, an->name);
                if (s) ptr = s->ptr;
                else ptr = alir_val_global(ctx->module, an->name, sem_get_node_type(ctx->sem, (ASTNode*)an->value));
            }
            if (ptr) {
                VarType target_type = ptr->type;
                if (target_type.ptr_depth > 0) target_type.ptr_depth--;

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
                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, val, ptr));
            }
            return val;
        }
        case NODE_BINARY_OP: return alir_gen_binary_op(ctx, (BinaryOpNode*)node);
        case NODE_UNARY_OP: return alir_gen_unary_op(ctx, (UnaryOpNode*)node);
        case NODE_INC_DEC: return alir_gen_inc_dec(ctx, (IncDecNode*)node);
        case NODE_CAST: return alir_gen_cast(ctx, (CastNode*)node);
        case NODE_SIZEOF:
        case NODE_ALIGNOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            AlirValue *dest = new_temp(ctx, sem_get_node_type(ctx->sem, node));
            AlirValue *type_val = alir_alloc(ctx->module, sizeof(AlirValue));
            type_val->kind = ALIR_VAL_TYPE; // tell LLVM it's a type 
            
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                type_val->type = sem_get_node_type(ctx->sem, sn->operand);
            } else {
                type_val->type = sn->target_type;
            }
            
            AlirOpcode op = (node->type == NODE_ALIGNOF) ? ALIR_OP_ALIGNOF : ALIR_OP_SIZEOF;
            emit(ctx, mk_inst(ctx->module, op, dest, type_val, NULL));
            return dest;
        }
        case NODE_TYPEOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            VarType op_type;
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                op_type = sem_get_node_type(ctx->sem, sn->operand);
            } else {
                op_type = sn->target_type;
            }
            return alir_const_int(ctx->module, op_type.base);
        }
        case NODE_DEFINED: {
            UnaryOpNode *un = (UnaryOpNode*)node;
            VarType t = { .base = TYPE_BOOL };
            AlirValue *dest = new_temp(ctx, t);
            
            // For defined(x), x is usually parsed as a VarRefNode. We extract its name.
            char *symbol_name = "";
            if (un->operand->type == NODE_VAR_REF) {
                symbol_name = ((VarRefNode*)un->operand)->name;
            }
            
            AlirValue *operand = alir_alloc(ctx->module, sizeof(AlirValue));
            operand->kind = ALIR_VAL_VAR;
            operand->val.str_val = alir_strdup(ctx->module, symbol_name);
            operand->type.base = TYPE_UNKNOWN;
            
            emit(ctx, mk_inst(ctx->module, ALIR_OP_DEFINED, dest, operand, NULL));
            return dest;
        }
        case NODE_MEMBER_ACCESS: return alir_gen_access(ctx, node);
        case NODE_INDEX_ACCESS: {
            IndexAccessNode *aa = (IndexAccessNode*)node;
            VarType elem_t = sem_get_node_type(ctx->sem, (ASTNode*)aa);
            VarType target_t = sem_get_node_type(ctx->sem, aa->target);
            
            // CRITICAL FIX: Trait access is a direct bitcast, no memory load!
            if (elem_t.base == TYPE_CLASS && target_t.base == TYPE_CLASS && 
                elem_t.class_name && target_t.class_name &&
                strcmp(elem_t.class_name, target_t.class_name) != 0) {
                
                AlirValue *target_val = alir_gen_expr(ctx, aa->target);
                if (!target_val) return NULL;
                
                AlirValue *casted = new_temp(ctx, elem_t);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_BITCAST, casted, target_val, NULL));
                return casted;
            }
            
            VarType t = target_t;
            if (t.base == TYPE_ENUM && ctx->sem->compiler_ctx->settings.inject_enum_as_cstring) {
                char *enum_name = NULL;
                if (aa->target->type == NODE_VAR_REF) enum_name = ((VarRefNode*)aa->target)->name;
                
                if (enum_name) {
                    SemSymbol *enum_sym = sem_symbol_lookup(ctx->sem, enum_name, NULL);
                    if (enum_sym && enum_sym->inner_scope) {
                        VarType str_type = (VarType){TYPE_CHAR, 1, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
                        AlirValue *dest = new_temp(ctx, str_type);
                        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, dest, NULL, NULL));
                        
                        AlirValue *cond = alir_gen_expr(ctx, aa->index);
                        if (!cond) cond = alir_const_int(ctx->module, 0);
                        
                        AlirBlock *end_bb = alir_add_block(ctx->module, ctx->current_func, "enum_str_end");
                        AlirBlock *default_bb = alir_add_block(ctx->module, ctx->current_func, "enum_str_def");
                        
                        AlirInst *sw = mk_inst(ctx->module, ALIR_OP_SWITCH, NULL, cond, alir_val_label(ctx->module, default_bb->label));
                        sw->cases = NULL;
                        AlirSwitchCase **tail = &sw->cases;
                        
                        SemSymbol *item = enum_sym->inner_scope->symbols;
                        while(item) {
                            AlirBlock *case_bb = alir_add_block(ctx->module, ctx->current_func, "enum_str_case");
                            AlirSwitchCase *sc = alir_alloc(ctx->module, sizeof(AlirSwitchCase));
                            sc->label = case_bb->label;
                            long val = 0;
                            alir_get_enum_value(ctx->module, enum_name, item->name, &val);
                            sc->value = val;
                            
                            *tail = sc;
                            tail = &sc->next;
                            item = item->next;
                        }
                        emit(ctx, sw);
                        
                        item = enum_sym->inner_scope->symbols;
                        AlirSwitchCase *sc_iter = sw->cases;
                        while(item && sc_iter) {
                            AlirBlock *case_bb = NULL;
                            AlirBlock *search = ctx->current_func->blocks;
                            while(search) { 
                                if (strcmp(search->label, sc_iter->label) == 0) { case_bb = search; break; }
                                search = search->next;
                            }
                            
                            ctx->current_block = case_bb;
                            AlirValue *glob = alir_module_add_string_literal(ctx->module, item->name, str_type, ctx->str_counter++);
                            emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, glob, dest));
                            emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, end_bb->label), NULL));
                            
                            item = item->next;
                            sc_iter = sc_iter->next;
                        }
                        
                        ctx->current_block = default_bb;
                        AlirValue *glob_def = alir_module_add_string_literal(ctx->module, "Unknown", str_type, ctx->str_counter++);
                        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, glob_def, dest));
                        emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, end_bb->label), NULL));
                        
                        ctx->current_block = end_bb;
                        AlirValue *res = new_temp(ctx, str_type);
                        emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, res, dest, NULL));
                        return res;
                    }
                }
            }
            return alir_gen_access(ctx, node);
        }
        case NODE_CALL: return alir_gen_call(ctx, (CallNode*)node);
        case NODE_METHOD_CALL: return alir_gen_method_call(ctx, (MethodCallNode*)node);

        
        default: {
            // [ROBUST FALLBACK]: Catch unimplemented expression nodes gracefully
            // By returning a dummy alloca for unrecognized types, we prevent 
            // ALICK's STORE validator from crashing on NULL ops.
            VarType t = sem_get_node_type(ctx->sem, node);
            if (t.base == TYPE_VOID) return NULL;
           
            AlirValue *dummy = new_temp(ctx, t);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, dummy, NULL, NULL));
            return dummy;
        }
    }
}
