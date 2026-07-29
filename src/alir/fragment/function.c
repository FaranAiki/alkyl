#include "alir.h"

void alir_gen_function_def(AlirCtx *ctx, FuncDefNode *fn, const char *class_name) {
    if (fn->is_flux) {
        alir_gen_flux_def(ctx, fn, class_name);
        return;
    }

    char func_name[256];
    if (fn->mangled_name) {
        int is_inherited = 0;
        if (class_name && fn->class_name) {
            if (strcmp(class_name, fn->class_name) != 0) {
                const char *dot = strrchr(class_name, '.');
                if (!dot || strcmp(dot + 1, fn->class_name) != 0) {
                    is_inherited = 1;
                }
            }
        }

        if (is_inherited) {
            // Inherited method: replace the original class name with the target class name
            char search_str[256];
            snprintf(search_str, sizeof(search_str), "_%s", fn->name);
            char *pos = strstr(fn->mangled_name, search_str);
            if (pos) {
                char *class_start = pos;
                while (class_start > fn->mangled_name && *(class_start - 1) != '_') {
                    class_start--;
                }

                char mangled_target[256];
                snprintf(mangled_target, sizeof(mangled_target), "%s", class_name);
                for (int i = 0; mangled_target[i]; i++) {
                    if (mangled_target[i] == '.') mangled_target[i] = '_';
                }

                if (strchr(class_name, '.')) {
                    snprintf(func_name, sizeof(func_name), "%s%s", mangled_target, pos);
                } else {
                    const char *top_ns = "main";
                    if (ctx->module && ctx->module->compiler_ctx) {
                    const char *dns = diag_get_namespace(ctx->module->compiler_ctx);
                        if (dns && strlen(dns) > 0) top_ns = dns;
                    }
                    snprintf(func_name, sizeof(func_name), "%s_%s%s", top_ns, mangled_target, pos);
                }
            } else {
                snprintf(func_name, sizeof(func_name), "%s", fn->mangled_name);
            }
        } else {
            // Direct method or top-level function: use mangled name as-is
            snprintf(func_name, sizeof(func_name), "%s", fn->mangled_name);
        }
    } else {
        if (class_name) {
            if (strcmp(fn->name, "init") == 0 || strcmp(fn->name, class_name) == 0) {
                snprintf(func_name, sizeof(func_name), "%s", class_name);
            } else {
                snprintf(func_name, sizeof(func_name), "%s_%s", class_name, fn->name);
            }
        } else {
            snprintf(func_name, sizeof(func_name), "%s", fn->name);
        }
    }

    debug_any("alir_gen_function_def fn->name=%s class_name=%s fn->mangled_name=%s -> func_name=%s\n", fn->name, class_name ? class_name : "NULL", fn->mangled_name ? fn->mangled_name : "NULL", func_name);

    ctx->current_func = alir_add_function(ctx->module, func_name, fn->ret_type, 0);
    ctx->current_func->is_varargs = fn->is_varargs;
    ctx->current_func->is_extern = fn->is_extern;
    ctx->current_func->is_pure = fn->is_pure;
    ctx->current_func->reason = fn->base.reason ? alir_strdup(ctx->module, fn->base.reason) : NULL;
    if (fn->cconv) ctx->current_func->cconv = alir_strdup(ctx->module, fn->cconv);

    if (class_name) {
        VarType this_t = {TYPE_CLASS, 1, alir_strdup(ctx->module, class_name)};
        alir_func_add_param(ctx->module, ctx->current_func, "this", this_t);
    }

    Parameter *p = fn->params;
    while(p) {
        alir_func_add_param(ctx->module, ctx->current_func, p->name, p->type);
        p = p->next;
    }

    if (!fn->has_body) return;

    ctx->current_block = alir_add_block(ctx->module, ctx->current_func, "entry");
    ctx->temp_counter = 0;
    ctx->symbols = NULL;

    int p_idx = 0;

    if (class_name) {
        VarType this_t = {TYPE_CLASS, 1, alir_strdup(ctx->module, class_name)};

        char pname[16]; snprintf(pname, sizeof(pname), "p%d", p_idx++);
        AlirValue *pval = alir_val_var(ctx->module, pname);
        pval->type = this_t;

        // [FIX] Actually allocate a local pointer for `this` to preserve standard calling conventions
        AlirValue *ptr = new_temp(ctx, this_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
        alir_add_symbol(ctx, "this", ptr, this_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, pval, ptr));
    }

    // For checking params
    p = fn->params;
    while(p) {
        AlirValue *ptr = new_temp(ctx, p->type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
        alir_add_symbol(ctx, p->name, ptr, p->type);

        char pname[16]; snprintf(pname, sizeof(pname), "p%d", p_idx++);
        AlirValue *pval = alir_val_var(ctx->module, pname);
        pval->type = p->type;
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, pval, ptr));

        p = p->next;
    }

    ASTNode *stmt = fn->body;
    while(stmt) { alir_gen_stmt(ctx, stmt); stmt = stmt->next; }

    if (ctx->current_block) {
        AlirInst *tail = ctx->current_block->tail;
        int has_term = tail && is_terminator(tail->op);

        if (!has_term) {
            ctx->current_line = fn->base.line;
            ctx->current_col = fn->base.col;

            if (strcmp(func_name, "main") == 0) {
                emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, alir_const_int(ctx->module, 0), NULL));
            } else if (fn->ret_type.base == TYPE_VOID || (class_name && (strcmp(fn->name, "init") == 0 || strcmp(fn->name, class_name) == 0))) {
                emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, NULL, NULL));
            } else {
                // Fallback for non-void functions that missed a return
                // Emit a dummy return to keep IR valid
                AlirValue *dummy = NULL;
                if (is_integer(fn->ret_type)) dummy = alir_const_int(ctx->module, 0);
                else if (is_numeric(fn->ret_type)) dummy = alir_const_float(ctx->module, 0.0);
                else if (is_pointer(fn->ret_type)) dummy = alir_const_int(ctx->module, 0); // null

                emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, dummy, NULL));
            }
        }
    }
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
            ret_type = (VarType){TYPE_CLASS, 0, alir_strdup(ctx->module, struct_name), 0, 0, NULL, NULL, 0, 0, 0, 0};
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
    if (alir_find_struct(ctx->module, target_name)) { printf("Found struct %s\n", target_name);
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

