#include "semantic.h"

// TODO check for in
void sem_check_for_in(SemanticCtx *ctx, ASTNode *node) {
    ForInNode *fn = (ForInNode*)node;
    sem_check_expr(ctx, fn->collection);
    ctx->in_loop++;
    
    VarType col_type = sem_get_node_type(ctx, fn->collection);
    VarType iter_type = col_type;
    
    if (iter_type.base == TYPE_CLASS && iter_type.class_name && strncmp(iter_type.class_name, "FluxCtx_", 8) == 0) {
        if (iter_type.fp_ret_type) {
            iter_type = *iter_type.fp_ret_type;
        } else {
            iter_type = (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
        }
    } else if (iter_type.array_size > 0) {
        iter_type.array_size = 0;
    } else if (iter_type.ptr_depth > 0) {
        iter_type.ptr_depth--;
    } else if (iter_type.base == TYPE_CLASS && iter_type.class_name && strcmp(iter_type.class_name, "string") == 0) {
        iter_type.base = TYPE_CHAR;
    } else if (is_integer(iter_type)) {
        // Allowed: integers act as valid iterators (0 to N-1) 
    } else {
        sem_error(ctx, node, "Cannot iterate over non-iterable type");
        iter_type = (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    }
    
    fn->iter_type = iter_type; 
    
    sem_scope_enter(ctx, 0, (VarType){0});
    SemSymbol *s = sem_symbol_add(ctx, fn->var_name, SYM_VAR, iter_type);
    s->is_initialized = 1; 
    
    sem_check_block(ctx, fn->body);
    sem_scope_exit(ctx);
    
    ctx->in_loop--;
}

void sem_check_unary_op_switch(SemanticCtx *ctx, ASTNode *node) {
    UnaryOpNode *un = (UnaryOpNode*)node;
    sem_check_expr(ctx, un->operand);
    VarType t = sem_get_node_type(ctx, un->operand);
    
    // Operator Overloading Check
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", un->is_suffix ? TOKEN_SUFFOP : TOKEN_PREFOP, un->op);
    SemSymbol *sym = sem_symbol_lookup(ctx, name_buf, NULL);
    if (sym && sym->kind == SYM_FUNC) {
        ASTNode *args = un->operand;
        args->next = NULL;
        SemSymbol *resolved = sem_resolve_overload(ctx, &args, NULL, sym, NULL);
        if (resolved) {
            un->overloaded_func_name = arena_strdup(ctx->compiler_ctx->arena, resolved->mangled_name ? resolved->mangled_name : resolved->name);
            sem_set_node_type(ctx, (ASTNode*)node, resolved->type);
            un->operand = args;
            un->operand->next = NULL;
            return;
        }
    }

    
    if (sem_get_node_tainted(ctx, un->operand)) {
        if (un->op != TOKEN_AND) {
            sem_set_node_tainted(ctx, node, 1);
        }
    }
    
    if (t.base == TYPE_VOID && t.ptr_depth == 0) {
         sem_error(ctx, node, "Operand of unary expression cannot be 'void'");
    }
    
    if (un->op == TOKEN_AND) { 
        t.ptr_depth++;
    } else if (un->op == TOKEN_STAR) { 
        if (t.ptr_depth > 0) t.ptr_depth--;
        else sem_error(ctx, node, "Cannot dereference non-pointer");
    } else if (un->op == TOKEN_NOT) {
        t = (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    }
    sem_set_node_type(ctx, node, t);
}

void sem_check_var_ref(SemanticCtx *ctx, ASTNode *node) {
    VarRefNode *ref = (VarRefNode*)node;
    
    void *err_val = hashmap_get(&ctx->compiler_ctx->error_table, ref->name);
    if (!err_val && strncmp(ref->name, "Err", 3) == 0) {
        int id = ctx->compiler_ctx->next_error_id++;
        hashmap_put(&ctx->compiler_ctx->error_table, strdup(ref->name), (void*)(intptr_t)(id + 1));
        err_val = (void*)(intptr_t)(id + 1);
    }
    
    if (err_val) {
        ref->is_error_id = 1;
        ref->error_id = (int)(intptr_t)err_val;
        sem_set_node_type(ctx, node, (VarType){TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    SemScope *found_in_scope = NULL;
    SemSymbol *sym = sem_symbol_lookup(ctx, ref->name, &found_in_scope);
    
    if (sym) {
        if (!sym->is_pristine) {
            sym->type.is_tainted = 1;
        } else if (sym->must_pristine) {
            sym->type.is_pristine = 1;
        }

        sem_set_node_type(ctx, node, sym->type);
        
        if (ctx->current_func_sym && ctx->current_func_sym->is_pure) {
            if (!sym->is_pure) {
                if (ctx->current_func_sym->must_pure) sem_error(ctx, node, "Pure function '%s' cannot use impure variable '%s'", ctx->current_func_sym->name, ref->name);
                ctx->current_func_sym->is_pure = false;
            }
            if (found_in_scope == ctx->global_scope) {
                if (ctx->current_func_sym->must_pure) sem_error(ctx, node, "Pure function '%s' cannot read global variable '%s'", ctx->current_func_sym->name, ref->name);
                ctx->current_func_sym->is_pure = false;
            }
        }

        if (!sym->is_pristine) {
            sem_set_node_tainted(ctx, node, 1);
        }

        if (sym->kind == SYM_VAR && !sym->is_initialized) {
            sem_error(ctx, node, "Use of uninitialized variable '%s'", ref->name);
        }

        if (found_in_scope && found_in_scope->is_class_scope) {
            ref->is_class_member = 1;
        } else {
            ref->is_class_member = 0;
        }
        return;
    } 

    // [FIX]: Check if we are inside a method scope by looking up "this"
    SemSymbol *this_sym = sem_symbol_lookup(ctx, "this", NULL);
    if (this_sym && this_sym->type.base == TYPE_CLASS && this_sym->type.class_name) {
        SemSymbol *class_sym = sem_symbol_lookup(ctx, this_sym->type.class_name, NULL);
        if (class_sym && class_sym->inner_scope) {
            SemScope *old_scope = ctx->current_scope;
            ctx->current_scope = class_sym->inner_scope;
            SemSymbol *member_sym = sem_symbol_lookup(ctx, ref->name, NULL);
            ctx->current_scope = old_scope;

            if (member_sym) {
                sem_set_node_type(ctx, node, member_sym->type);
                ref->is_class_member = 1;
                return;
            }
        }
    }

    sem_error(ctx, node, "Undefined variable '%s'", ref->name);
    sem_set_node_type(ctx, node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
}

void sem_check_array_access(SemanticCtx *ctx, ASTNode *node) {
    ArrayAccessNode *aa = (ArrayAccessNode*)node;
    sem_check_expr(ctx, aa->target);
    sem_check_expr(ctx, aa->index);
    
    if (sem_get_node_tainted(ctx, aa->target)) {
        sem_set_node_tainted(ctx, node, 1);
    }
    
    VarType t = sem_get_node_type(ctx, aa->target);
    
    // Check for union access by type
    if (t.base == TYPE_CLASS && t.class_name) {
        SemSymbol *class_sym = sem_symbol_lookup(ctx, t.class_name, NULL);
        if (class_sym && class_sym->is_union && aa->index->type == NODE_LITERAL) {
            VarType index_type = sem_get_node_type(ctx, aa->index);
            if (class_sym->inner_scope) {
                SemSymbol *f = class_sym->inner_scope->symbols;
                while (f) {
                    if (f->kind == SYM_VAR && sem_types_are_compatible(ctx, f->type, index_type)) {
                        MemberAccessNode ma;
                        memset(&ma, 0, sizeof(MemberAccessNode));
                        ma.base.type = NODE_MEMBER_ACCESS;
                        ma.base.line = node->line;
                        ma.base.col = node->col;
                        ma.object = aa->target;
                        ma.member_name = arena_strdup(ctx->compiler_ctx->arena, f->name);
                        
                        sem_set_node_type(ctx, node, f->type);
                        memcpy(node, &ma, sizeof(MemberAccessNode));
                        return;
                    }
                    f = f->next;
                }
            }
            sem_error(ctx, node, "Union does not have a member of the requested type");
            sem_set_node_type(ctx, node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            return;
        }
    }
    
    // Check for trait access (composition)
    if (t.base == TYPE_CLASS && t.class_name && aa->index->type == NODE_VAR_REF) {
        char *trait_name = ((VarRefNode*)aa->index)->name;
        SemSymbol *class_sym = sem_symbol_lookup(ctx, t.class_name, NULL);
        if (class_sym && class_sym->trait_count > 0) {
            for (int i = 0; i < class_sym->trait_count; i++) {
                if (strcmp(class_sym->traits[i], trait_name) == 0) {
                    // Valid composition access!
                    VarType trait_t = t;
                    trait_t.class_name = arena_strdup(ctx->compiler_ctx->arena, trait_name);
                    sem_set_node_type(ctx, node, trait_t);
                    return;
                }
            }
        }
    }
    
    if (t.array_size > 0) t.array_size = 0;
    else if (t.ptr_depth > 0) t.ptr_depth--;
    else if (t.base == TYPE_ENUM || t.base == TYPE_ARRAY || (t.base == TYPE_CLASS && t.class_name && (strcmp(t.class_name, "string") == 0 || strcmp(t.class_name, "vector") == 0 || strcmp(t.class_name, "hashmap") == 0))) {
         // for now wait!
         sem_set_node_type(ctx, node, (VarType){ .base = TYPE_CLASS, .class_name = (char*)"string" });
         return;
    }
    else { 
        sem_error(ctx, node, "Type is not a pointer, array, string, vector, hashmap, or enum");
        t = (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    }
    sem_set_node_type(ctx, node, t);
}
