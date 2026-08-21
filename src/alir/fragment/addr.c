#include "alir.h"

AlirValue* alir_gen_addr_var_ref(AlirCtx *ctx, ASTNode *node) {
    VarRefNode *vn = (VarRefNode*)node;
    if (vn->is_class_member) {
        AlirSymbol *this_sym = alir_find_symbol(ctx, "this");
        if (!this_sym) return NULL;

        char *class_name = this_sym->type.class_name;

        int idx = -1;
        VarType field_type = {TYPE_AUTO, 0, NULL};
        AlirStruct *st = alir_find_struct(ctx->module, class_name);
        if (st) {
            AlirField *f = st->fields;
            while(f) {
                if (streq_lit(f->name, vn->name)) {
                    idx = f->index;
                    field_type = f->type;
                    break;
                }
                f = f->next;
            }
        }
        if (idx == -1) {
            idx = alir_robust_get_field_index(ctx, class_name, vn->name);
            
            if (class_name && streq_lit(class_name, "string")) {
                if (streq_lit(vn->name, "data")) {
                    field_type = (VarType){ .base = TYPE_CHAR, .ptr_depth = 1 };
                } else if (streq_lit(vn->name, "len")) {
                    field_type = (VarType){ .base = TYPE_UNSIGNED_INT, .ptr_depth = 0 };
                }
            }
            
            if (idx != -1 && field_type.base == TYPE_AUTO) {
                AlirStruct *search = ctx->module->structs;
                while (search) {
                    AlirField *f = search->fields;
                    while(f) {
                        if (streq_lit(f->name, vn->name)) { field_type = f->type; break; }
                        f = f->next;
                    }
                    search = search->next;
                }
            }
        }
        if (idx == -1) idx = 0;

        AlirValue *this_ptr = new_temp(ctx, this_sym->type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, this_ptr, this_sym->ptr, NULL));

        VarType mem_type = sem_get_node_type(ctx->sem, node);
        if (field_type.base != TYPE_AUTO) mem_type = field_type;
        mem_type.ptr_depth++;

        AlirValue *res = new_temp(ctx, mem_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, res, this_ptr, alir_const_int(ctx->module, idx)));
        return res;
    }

    AlirSymbol *sym = alir_find_symbol(ctx, vn->name);
    if (sym) {
        if (vn->is_implicit_deref) {
            AlirValue *loaded = new_temp(ctx, sym->type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, loaded, sym->ptr, NULL));
            return loaded;
        }
        if (sym->type.is_tainted && sym->type.ptr_depth == 0) {
            VarType val_type = sym->type;
            val_type.is_tainted = 0;
            val_type.ptr_depth++;
            AlirValue *val_ptr = new_temp(ctx, val_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, val_ptr, sym->ptr, alir_const_int(ctx->module, 1)));
            return val_ptr;
        }
        return sym->ptr;
    }

    SemSymbol *glob_sym = sem_symbol_lookup(ctx->sem, vn->name, NULL);
    if (glob_sym && glob_sym->kind == SYM_VAR) {
        VarType t = sem_get_node_type(ctx->sem, node);
        t.ptr_depth++;
        debug_alir("GLOBAL VAR ADDR: %s\n", vn->name); return alir_val_global(ctx->module, glob_sym->mangled_name ? glob_sym->mangled_name : vn->name, t);
    }

    // Implicit this indexing
    AlirSymbol *this_sym = alir_find_symbol(ctx, "this");
    if (this_sym && this_sym->type.class_name) {
        int idx = -1;
        VarType field_type = {TYPE_AUTO, 0, NULL};
        AlirStruct *st = alir_find_struct(ctx->module, this_sym->type.class_name);
        if (st) {
            AlirField *f = st->fields;
            while(f) {
                if (streq_lit(f->name, vn->name)) {
                    idx = f->index;
                    field_type = f->type;
                    break;
                }
                f = f->next;
            }
        }

        if (this_sym->type.class_name && streq_lit(this_sym->type.class_name, "string")) {
            if (streq_lit(vn->name, "data")) {
                idx = 1;
                field_type = (VarType){ .base = TYPE_CHAR, .ptr_depth = 1 };
            } else if (streq_lit(vn->name, "len")) {
                idx = 0;
                field_type = (VarType){ .base = TYPE_UNSIGNED_INT, .ptr_depth = 0 };
            }
        }
        
        if (idx == -1) {
            AlirStruct *search = ctx->module->structs;
            while (search) {
                AlirField *f = search->fields;
                while(f) {
                    if (streq_lit(f->name, vn->name)) { idx = f->index; field_type = f->type; break; }
                    f = f->next;
                }
                if (idx != -1) break;
                search = search->next;
            }
        }

        if (idx != -1) {
            AlirValue *this_ptr = new_temp(ctx, this_sym->type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, this_ptr, this_sym->ptr, NULL));

            VarType mem_type = sem_get_node_type(ctx->sem, node);
            if (field_type.base != TYPE_AUTO) mem_type = field_type;
            mem_type.ptr_depth++;

            AlirValue *res = new_temp(ctx, mem_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, res, this_ptr, alir_const_int(ctx->module, idx)));
            return res;
        }
    }
    return NULL;
}

AlirValue* alir_gen_addr_member_access(AlirCtx *ctx, ASTNode *node) {
    MemberAccessNode *ma = (MemberAccessNode*)node;
    VarType obj_t = sem_get_node_type(ctx->sem, ma->object);
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

    int idx = -1;
    VarType field_type = {TYPE_AUTO, 0, NULL};

    if (class_name && streq_lit(class_name, "string")) {
        if (streq_lit(ma->member_name, "data")) {
            idx = 1;
            field_type = (VarType){ .base = TYPE_CHAR, .ptr_depth = 1 };
        } else if (streq_lit(ma->member_name, "len")) {
            idx = 0;
            field_type = (VarType){ .base = TYPE_UNSIGNED_INT, .ptr_depth = 0 };
        }
    }

    if (class_name && idx == -1) {
        AlirStruct *st = alir_find_struct(ctx->module, class_name);
        if (st) {
            AlirField *f = st->fields;
            while(f) {
                if (streq_lit(f->name, ma->member_name)) {
                    idx = f->index;
                    field_type = f->type;
                    break;
                }
                f = f->next;
            }
        }
    }
    if (idx == -1) {
        // Fallback global search
        AlirStruct *search = ctx->module->structs;
        while (search) {
            AlirField *f = search->fields;
            while(f) {
                if (streq_lit(f->name, ma->member_name)) {
                    idx = f->index;
                    field_type = f->type;
                    break;
                }
                f = f->next;
            }
            if (idx != -1) break;
            search = search->next;
        }
    }
    if (idx == -1) {
        idx = 0;
        field_type = (VarType){TYPE_AUTO, 0, NULL};
    }

    field_type.ptr_depth++; // It's a pointer to the field
    AlirValue *res = new_temp(ctx, field_type);
  // TODO I don't think this is true
    emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, res, base_ptr, alir_const_int(ctx->module, idx)));
    return res;
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
        
        if (class_name && streq_lit(class_name, "string")) {
            if (streq_lit(ma->member_name, "data")) {
                field_type = (VarType){ .base = TYPE_CHAR, .ptr_depth = 1 };
            } else if (streq_lit(ma->member_name, "len")) {
                field_type = (VarType){ .base = TYPE_UNSIGNED_INT, .ptr_depth = 0 };
            }
        }
        
        if (class_name && field_type.base == TYPE_AUTO) {
            AlirStruct *st = alir_find_struct(ctx->module, class_name);
            if (st) {
                AlirField *f = st->fields;
                while(f) {
                    if (streq_lit(f->name, ma->member_name)) { field_type = f->type; break; }
                    f = f->next;
                }
            }
        }

        if (field_type.base == TYPE_AUTO) {
            AlirStruct *search = ctx->module->structs;
            while (search) {
                AlirField *f = search->fields;
                while(f) {
                    if (streq_lit(f->name, ma->member_name)) { field_type = f->type; break; }
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

    if (node->type == NODE_UNARY_OP) {
        UnaryOpNode *un = (UnaryOpNode*)node;
        if (un->op == TOKEN_STAR) {
            // The address of `*ptr` is just the value of `ptr`.
            return alir_gen_expr(ctx, un->operand);
        }
    }

    if (node->type == NODE_CALL || node->type == NODE_METHOD_CALL) {
        AlirValue *rval = alir_gen_expr(ctx, node);
        if (rval) {
            VarType t = sem_get_node_type(ctx->sem, node);
            AlirValue *ptr = new_temp(ctx, t);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
            emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, rval, ptr));
            return ptr;
        }
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
            case TYPE_LONG_LONG:
                return alir_const_long_long(ctx->module, ln->val.long_long_val);
            case TYPE_UNSIGNED_LONG:
                return alir_const_unsigned_long(ctx->module, ln->val.long_val);
            case TYPE_UNSIGNED_LONG_LONG:
                return alir_const_unsigned_long_long(ctx->module, ln->val.unsigned_long_val);
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
            case TYPE_BOOL:
                return alir_const_bool(ctx->module, ln->val.long_val);
            default: break; // TODO here
        }
    }

    if ((ln->var_type.base == TYPE_CLASS && ln->var_type.class_name && streq_lit(ln->var_type.class_name, "string")) || (ln->var_type.base == TYPE_CHAR && ln->var_type.ptr_depth > 0)) {
        if (!ln->val.str_val || (long)ln->val.str_val <= 0x1000) {
            return alir_const_int(ctx->module, ln->val.long_val);
        }

        AlirValue *glob = alir_module_add_string_literal(ctx->module, ln->val.str_val, ln->var_type);
        if (ln->var_type.base == TYPE_CLASS && streq_lit(ln->var_type.class_name, "string")) {
            AlirValue *val = new_temp(ctx, ln->var_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, glob, NULL));
            return val;
        }
        return glob;
    }

    if (ln->var_type.base == TYPE_BOOL) {
        return alir_const_int(ctx->module, ln->val.long_val);
    }

    if ((ln->var_type.base == TYPE_UNKNOWN || ln->var_type.base == TYPE_VOID) && ln->var_type.ptr_depth >= 1) {
        AlirValue *v = alir_alloc(ctx->module, sizeof(AlirValue));
        v->kind = ALIR_VAL_CONST;
        v->type = ln->var_type;
        v->val.int_val = 0;
        return v;
    }

    // Fallback for empty/unhandled literals
    // TODO fix this
    debug_alir("Unknown literal! type base %d", ln->var_type.base);
    return alir_const_int(ctx->module, 0);
}

AlirValue* alir_gen_var_ref(AlirCtx *ctx, VarRefNode *vn) {
    if (vn->is_error_id) {
        return alir_const_int(ctx->module, vn->error_id);
    }

    AlirValue *fold_val = hashmap_get(&ctx->const_fold_map, vn->name);
    if (!fold_val) {
        fold_val = hashmap_get(&ctx->module->const_fold_map, vn->name);
    }
    if (fold_val) return fold_val;

    AlirValue *ptr = alir_gen_addr(ctx, (ASTNode*)vn);
    if (!ptr) {
        SemSymbol *sym = sem_symbol_lookup(ctx->sem, vn->name, NULL);
        if (sym && vn->mangled_name) {
            SemSymbol *s = sym;
            while (s) {
                if (s->mangled_name && streq_lit(s->mangled_name, vn->mangled_name)) {
                    sym = s;
                    break;
                }
                s = s->overload_next;
            }
        }

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
            debug_alir("GLOBAL VAR ADDR: %s\n", vn->name); return alir_val_global(ctx->module, sym->mangled_name ? sym->mangled_name : vn->name, t);
                } else if (sym && sym->kind == SYM_CLASS) {
            unsigned int hash = 5381;
            char *str = sym->name;
            int c;
            while ((c = *str++)) hash = ((hash << 5) + hash) + c;
            return alir_const_int(ctx->module, hash);
        } else if (sym && sym->kind == SYM_VAR) {
            VarType t = sem_get_node_type(ctx->sem, (ASTNode*)vn);
            t.ptr_depth++; // Make it a pointer type because it's an address
            ptr = alir_val_global(ctx->module, sym->mangled_name ? sym->mangled_name : vn->name, t);
        } else if (sym && (sym->kind == SYM_TEMPLATE || sym->kind == SYM_NAMESPACE)) {
            // Naked template or namespace reference. Just return 0 to avoid JIT panic.
            return alir_const_int(ctx->module, 0);
        } else {
            return NULL; // Safety guard against unresolved allocas
        }
    }

    // Special Enum Handling: bare enum variant references resolve to their constant value
    if (ptr) {
        VarType t = sem_get_node_type(ctx->sem, (ASTNode*)vn);
        if (t.base == TYPE_ENUM && t.class_name) {
            long val = 0;
            if (alir_get_enum_value(ctx->module, t.class_name, vn->name, &val)) {
                return alir_const_int(ctx->module, val);
            }
        }
    }

    // Get precise type from Semantics
    VarType t = sem_get_node_type(ctx->sem, (ASTNode*)vn);

    AlirSymbol *asym = alir_find_symbol(ctx, vn->name);
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

