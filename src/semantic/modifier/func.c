/**
 * @file func.c
 * @brief Function-related semantic checking implementation.
 */
#include "func.h"

void sem_check_method_call(SemanticCtx *ctx, MethodCallNode *node) {
    sem_check_expr(ctx, node->object);
    VarType obj_type = sem_get_node_type(ctx, node->object);

    if (sem_get_node_tainted(ctx, node->object)) {
        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
    }

    if (obj_type.base == TYPE_UNKNOWN) {
        sem_error(ctx, (ASTNode*)node, "Unknown type");
        return;
    }

    debug_semantic("sem_check_method_call: method='%s', obj_base=%d, obj_class='%s'\n", node->method_name, obj_type.base, obj_type.class_name ? obj_type.class_name : "(null)");

    int ufcs_fallback = 0;
    
    const char *primitive_class_name = NULL;
    if (obj_type.base != TYPE_CLASS && obj_type.ptr_depth == 0) {
        if (obj_type.base == TYPE_INT) primitive_class_name = "int";
        else if (obj_type.base == TYPE_CHAR) primitive_class_name = "char";
        else if (obj_type.base == TYPE_BOOL) primitive_class_name = "bool";
        else if (obj_type.base == TYPE_SINGLE) primitive_class_name = "single";
        else if (obj_type.base == TYPE_DOUBLE) primitive_class_name = "double";
    }

    if ((obj_type.base == TYPE_CLASS && obj_type.class_name) || primitive_class_name) {
        if (!sem_lookup_class_call(ctx, node)) {
            ufcs_fallback = 1;
        }
    } else if (obj_type.base == TYPE_NAMESPACE && obj_type.class_name) {
        SemSymbol *ns_sym = sem_symbol_lookup(ctx, obj_type.class_name, NULL);
        if (!ns_sym || ns_sym->kind != SYM_NAMESPACE) {
            sem_error(ctx, (ASTNode*)node, "'%s' is not a namespace", obj_type.class_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            return;
        }

        int found = 0;
        if (ns_sym->inner_scope) {
            SemSymbol *member = NULL;
            if (ns_sym->inner_scope->symbol_map) {
                member = hashmap_get((HashMap*)ns_sym->inner_scope->symbol_map, node->method_name);
            } else {
                member = ns_sym->inner_scope->symbols;
                while (member && !streq_lit(member->name, node->method_name)) {
                    member = member->next;
                }
            }
            if (member) {
                    if (ctx->current_func_sym && ctx->current_func_sym->is_pure) {
                        if (member->kind == SYM_FUNC && !member->is_pure) {
                            if (ctx->current_func_sym->must_pure) {
                                sem_error(ctx, (ASTNode*)node, "Pure function '%s' cannot call impure method '%s'", ctx->current_func_sym->name, member->name);
                            }
                            ctx->current_func_sym->is_pure = false;
                        }
                    }

                    if (ctx->current_func_sym && ctx->current_func_sym->is_total) {
                        if (member->kind == SYM_FUNC && !member->is_total) {
                            if (ctx->current_func_sym->must_total) {
                                sem_error(ctx, (ASTNode*)node, "Total function '%s' cannot call partial method '%s'", ctx->current_func_sym->name, member->name);
                            }
                            ctx->current_func_sym->is_total = false;
                        }
                    }

                    if (member->kind == SYM_FUNC && !member->is_pristine) {
                        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
                    }

                    if (member->kind == SYM_FUNC && member->is_macro) {
                        char full_name[512];
                        snprintf(full_name, sizeof(full_name), "%s.%s", obj_type.class_name, member->name);

                        ASTNode *saved_args = node->args;
                        int saved_line = node->base.line;
                        int saved_col = node->base.col;

                        CallNode *call = (CallNode*)node;
                        call->base.type = NODE_CALL;
                        call->base.line = saved_line;
                        call->base.col = saved_col;

                        VarRefNode *target = arena_alloc_type(ctx->compiler_ctx->arena, VarRefNode);
                        target->base.type = NODE_VAR_REF;
                        target->base.line = saved_line;
                        target->base.col = saved_col;
                        target->name = arena_strdup(ctx->compiler_ctx->arena, full_name);
                        target->is_class_member = 0;

                        call->name = target->name;
                        call->mangled_name = NULL;
                        call->args = saved_args;
                        call->target = (ASTNode*)target;

                        extern void sem_check_call(SemanticCtx *ctx, CallNode *node);
                        sem_check_call(ctx, call);
                        return;
                    }
                    else if (member->kind == SYM_FUNC) {
                        if (member->is_flux) {
                            char buf[512];
                            snprintf(buf, sizeof(buf), "FluxCtx_%s", member->mangled_name ? member->mangled_name : member->name);
                            VarType flux_type = {TYPE_CLASS, 0, arena_strdup(ctx->compiler_ctx->arena, buf), 0, 0, NULL, NULL, 0, 0, 0, 0};
                            flux_type.fp_ret_type = arena_alloc_type(ctx->compiler_ctx->arena, VarType);
                            *flux_type.fp_ret_type = member->type;
                            sem_set_node_type(ctx, (ASTNode*)node, flux_type);
                        } else {
                            sem_set_node_type(ctx, (ASTNode*)node, member->type);
                        }
                        node->owner_class = ns_sym->name;
                        node->is_static = 1;
                        found = 1;
                    }
                    else if (member->kind == SYM_VAR && member->type.is_func_ptr) {
                        sem_set_node_type(ctx, (ASTNode*)node, *member->type.fp_ret_type);
                        found = 1;
                    }
                    else if (member->kind == SYM_CLASS) {
                        char full_name[512];
                        snprintf(full_name, sizeof(full_name), "%s.%s", obj_type.class_name, member->name);

                        ASTNode *saved_args = node->args;
                        int saved_line = node->base.line;
                        int saved_col = node->base.col;

                        CallNode *call = (CallNode*)node;
                        call->base.type = NODE_CALL;
                        call->base.line = saved_line;
                        call->base.col = saved_col;

                        VarRefNode *target = arena_alloc_type(ctx->compiler_ctx->arena, VarRefNode);
                        target->base.type = NODE_VAR_REF;
                        target->base.line = saved_line;
                        target->base.col = saved_col;
                        target->name = arena_strdup(ctx->compiler_ctx->arena, full_name);
                        target->is_class_member = 0;

                        call->name = target->name;
                        call->mangled_name = NULL;
                        call->args = saved_args;
                        call->target = (ASTNode*)target;

                        extern void sem_check_call(SemanticCtx *ctx, CallNode *node);
                        sem_check_call(ctx, call);
                        return;
                    }

                    if (found) {
                        if (member->kind == SYM_FUNC) {
                            SemSymbol *resolved = sem_resolve_overload(ctx, &node->args, NULL, member, (ASTNode*)node);
                            if (resolved) {
                                node->mangled_name = resolved->mangled_name;
                            }
                        } else {
                            // Var func ptr call etc
                            ASTNode **curr_arg = &node->args;
                            while(*curr_arg) {
                                sem_check_expr(ctx, *curr_arg);
                                curr_arg = &(*curr_arg)->next;
                            }
                        }
                        goto done_ns_method_search;
                    }
            }
        }

        done_ns_method_search:
        if (!found) {
             ufcs_fallback = 1;
        }
    } else {
        ufcs_fallback = 1;
    }

    if (ufcs_fallback) {
        if (ctx->compiler_ctx->settings.resolve_method_call_as_call) {
            ASTNode *obj = node->object;
            char *meth = node->method_name;
            ASTNode *args = node->args;

            obj->next = args;

            VarRefNode *vr = arena_alloc_type(ctx->compiler_ctx->arena, VarRefNode);
            vr->base.type = NODE_VAR_REF;
            vr->base.line = node->base.line;
            vr->base.col = node->base.col;
            vr->name = meth;

            CallNode *call = (CallNode*)node;
            call->base.type = NODE_CALL;
            call->name = meth;
            call->mangled_name = NULL;
            call->args = obj;
            call->target = (ASTNode*)vr;

            extern void sem_check_call(SemanticCtx *ctx, CallNode *node);
            sem_check_call(ctx, call);
        } else {
            if (obj_type.base == TYPE_CLASS && obj_type.class_name) {
                sem_error(ctx, (ASTNode*)node, "Method '%s' not found in class '%s'", node->method_name, obj_type.class_name);
            } else if (obj_type.base == TYPE_NAMESPACE && obj_type.class_name) {
                sem_error(ctx, (ASTNode*)node, "Function '%s' not found in namespace '%s'", node->method_name, obj_type.class_name);
            } else {
                sem_error(ctx, (ASTNode*)node, "Cannot call method on non-class/non-namespace type");
            }
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        }
    }
}

void sem_check_func_def(SemanticCtx *ctx, FuncDefNode *node) {
    if (!node) return;

    if (node->ret_type.base == TYPE_CLASS && node->ret_type.class_name) {
        SemSymbol *sym = sem_symbol_lookup(ctx, node->ret_type.class_name, NULL);
        if (sym && sym->kind == SYM_TEMPLATE) {
            CompoundNode *cn = sym->template_node;
            char expected_types[256] = "";
            size_t pos = 0;
            for (int i=0; i<cn->num_type_params; i++) {
                pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, "%s", cn->type_params[i]);
                if (i < cn->num_type_params - 1 && pos < sizeof(expected_types) - 1) {
                    pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, ", ");
                }
            }
            sem_error(ctx, (ASTNode*)node, "'%s' needs types [%s]", node->ret_type.class_name, expected_types);
            node->ret_type.base = TYPE_UNKNOWN;
        }
    }

    sem_scope_enter(ctx, 1, node->ret_type);

    SemSymbol *old_func = ctx->current_func_sym;
    ctx->current_func_sym = sem_symbol_lookup(ctx, node->name, NULL);

    if (node->class_name) {
        VarType this_type = {TYPE_CLASS, 1, arena_strdup(ctx->compiler_ctx->arena, node->class_name), 0, 0, NULL, NULL, 0, 0, 0, 0};
        
        if (streq_lit(node->class_name, "int")) { this_type.base = TYPE_INT; this_type.class_name = NULL; }
        else if (streq_lit(node->class_name, "char")) { this_type.base = TYPE_CHAR; this_type.class_name = NULL; }
        else if (streq_lit(node->class_name, "bool")) { this_type.base = TYPE_BOOL; this_type.class_name = NULL; }
        else if (streq_lit(node->class_name, "single")) { this_type.base = TYPE_SINGLE; this_type.class_name = NULL; }
        else if (streq_lit(node->class_name, "double")) { this_type.base = TYPE_DOUBLE; this_type.class_name = NULL; }
        
        SemSymbol *this_sym = sem_symbol_add(ctx, "this", SYM_VAR, this_type);
        if (!node->is_mutable) {
            this_sym->is_mutable = false;
        }
    }

    Parameter *p = node->params;
    while (p) {
        if (p->type.base == TYPE_CLASS && p->type.class_name) {
            SemSymbol *sym = sem_symbol_lookup(ctx, p->type.class_name, NULL);
            if (sym && sym->kind == SYM_TEMPLATE) {
                CompoundNode *cn = sym->template_node;
                char expected_types[256] = "";
                size_t pos = 0;
                for (int i=0; i<cn->num_type_params; i++) {
                    pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, "%s", cn->type_params[i]);
                    if (i < cn->num_type_params - 1 && pos < sizeof(expected_types) - 1) {
                        pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, ", ");
                    }
                }
                sem_error(ctx, (ASTNode*)node, "'%s' needs types [%s]", p->type.class_name, expected_types);
                p->type.base = TYPE_UNKNOWN;
            }
        }
        if (p->name) {
            SemSymbol *s = sem_symbol_add(ctx, p->name, SYM_VAR, p->type);
            s->is_initialized = 1;
            s->is_pure = p->is_pure;
            s->must_pure = p->has_explicit_pure;
            s->is_pristine = p->is_pristine;
            s->must_pristine = p->has_explicit_pristine;
        }
        p = p->next;
    }

    if (!node->is_macro) {
        sem_check_block(ctx, node->body);
    }
    sem_scope_exit(ctx);

    ctx->current_func_sym = old_func;
}

#include <string.h>

static int get_type_rank(VarType t) {
    if (t.ptr_depth > 0 || t.array_size > 0 || t.array_depth > 0) return 100;
    switch (t.base) {
        case TYPE_BOOL: return 1;
        case TYPE_CHAR: return 2;
        case TYPE_SHORT: return 3;
        case TYPE_INT: return 4;
        case TYPE_LONG: return 5;
        case TYPE_LONG_LONG: return 6;
        case TYPE_SINGLE: return 7;
        case TYPE_DOUBLE: return 8;
        case TYPE_LONG_DOUBLE: return 9;
        default: return 0;
    }
}

void sem_check_call(SemanticCtx *ctx, CallNode *node) {
    debug_semantic("sem_check_call: name='%s', ns='%s'\n", node->name ? node->name : "(null)", ctx->compiler_ctx ? diag_get_namespace(ctx->compiler_ctx) : "(null)");
    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return;

    SemSymbol *sym = NULL;
    if (node->target) {
        int before_type = node->target->type;
        sem_check_expr(ctx, node->target);
        int after_type = node->target->type;
        if (before_type != after_type) {
            debug_semantic("sem_check_call: TARGET MODIFIED! node=%p before=%d after=%d\n", (void*)node, before_type, after_type);
        }
        if (node->target->type == NODE_TEMPLATE_INSTANTIATION) {
            // target->target was updated to VarRef inside sem_check_expr
            TemplateInstNode *ti = (TemplateInstNode*)node->target;
            if (ti->target->type == NODE_VAR_REF) {
                node->name = ((VarRefNode*)ti->target)->name;
            } else if (ti->target->type == NODE_MEMBER_ACCESS) {
                MethodCallNode *mc = (MethodCallNode*)node;
                MemberAccessNode *ma = (MemberAccessNode*)ti->target;
                mc->base.type = NODE_METHOD_CALL;
                mc->object = ma->object;
                mc->method_name = ma->member_name; // the mangled name
                mc->mangled_name = NULL;
                mc->owner_class = NULL;
                mc->is_static = 0;
                sem_check_method_call(ctx, mc);
                return;
            }
        } else if (node->target->type == NODE_VAR_REF) {
            node->name = ((VarRefNode*)node->target)->name;
        } else if (node->target->type == NODE_MEMBER_ACCESS) {
            node->name = ((MemberAccessNode*)node->target)->member_name;
        } else if (node->target->type == NODE_METHOD_CALL) {
            debug_semantic("CallNode has MethodCall target! method_name=%s\n", ((MethodCallNode*)node->target)->method_name);
        }
    }

    if (node->name) {
        char *bracket = strchr(node->name, '[');
        if (bracket) {
            char mangled[512];
            snprintf(mangled, sizeof(mangled), "%s", node->name);
            for (int i=0; mangled[i]; i++) {
                if (mangled[i] == '[') mangled[i] = '_';
                else if (mangled[i] == ']') mangled[i] = '\0';
                else if (mangled[i] == ',' || mangled[i] == ' ') mangled[i] = '_';
            }
            // Remove double underscores just in case `, ` became `__`
            char final_mangled[512];
            int j = 0;
            for (int i=0; mangled[i] && j < 511; i++) {
                if (mangled[i] == '_' && mangled[i+1] == '_') continue;
                final_mangled[j++] = mangled[i];
            }
            final_mangled[j] = '\0';
            node->name = arena_strdup(ctx->compiler_ctx->arena, final_mangled);
        }
        sym = sem_symbol_lookup(ctx, node->name, NULL);
        if (sym) {
        }
    }
    if (!sym) {
        if (node->name) {
            sem_error(ctx, (ASTNode*)node, "Undefined function or class '%s'", node->name);
        } else {
            debug_semantic("Cannot call non-function type at line %d col %d, node type %d, target type %d\n", node->base.line, node->base.col, node->base.type, node->target ? (int)node->target->type : -1); sem_error(ctx, (ASTNode*)node, "Cannot call non-function type");
        }
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    if (ctx->current_func_sym && ctx->current_func_sym->is_pure) {
        if (!sym->is_pure) {
            if (ctx->current_func_sym->must_pure) {
                if (sym->kind == SYM_FUNC) {
                    sem_error(ctx, (ASTNode*)node, "Pure function '%s' cannot call impure function '%s'", ctx->current_func_sym->name, sym->name);
                } else if (sym->kind == SYM_CLASS) {
                    sem_error(ctx, (ASTNode*)node, "Pure function '%s' cannot use impure class '%s'", ctx->current_func_sym->name, sym->name);
                }
            }
            ctx->current_func_sym->is_pure = false;
        }
    }

    if (ctx->current_func_sym && ctx->current_func_sym->is_total) {
        if (!sym->is_total && sym->kind == SYM_FUNC) {
            if (ctx->current_func_sym->must_total) {
                sem_error(ctx, (ASTNode*)node, "Total function '%s' cannot call partial function '%s'", ctx->current_func_sym->name, sym->name);
            }
            ctx->current_func_sym->is_total = false;
        }
    }

    if (sym->kind == SYM_FUNC) {
        if (!sym->is_pristine) {
            sem_set_node_tainted(ctx, (ASTNode*)node, 1);
        }
    }

    if (sym->kind == SYM_FUNC) {
        SemSymbol *resolved = sem_resolve_overload(ctx, &node->args, NULL, sym, (ASTNode*)node);
        if (resolved) {
            node->mangled_name = resolved->mangled_name;
            sym = resolved; // Update sym to the resolved one
            if (node->target && node->target->type == NODE_VAR_REF) {
                ((VarRefNode*)node->target)->mangled_name = resolved->mangled_name;
                sem_set_node_type(ctx, node->target, resolved->type);
            }
        }
    }

    if (sym->kind == SYM_TEMPLATE) {
        CompoundNode *cn = sym->template_node;

        // Typecheck arguments so we can deduce types
        ASTNode *arg = node->args;
        while (arg) {
            sem_check_expr(ctx, arg);
            arg = arg->next;
        }

        if (cn->body && cn->body->type == NODE_FUNC_DEF && cn->body->next == NULL) {
            FuncDefNode *fn = (FuncDefNode*)cn->body;
            VarType inferred_types[16];
            int inferred_flags[16] = {0}; // 1 if deduced, 0 otherwise
            int deduction_failed = 0;

            Parameter *param = fn->params;
            arg = node->args;

            while (param && arg) {
                if (param->type.base == TYPE_CLASS && param->type.class_name && param->type.ptr_depth == 0 && param->type.array_depth == 0) {
                    for (int i = 0; i < cn->num_type_params; i++) {
                        if (streq_lit(param->type.class_name, cn->type_params[i])) {
                            VarType arg_t = sem_get_node_type(ctx, arg);

                            if (!inferred_flags[i]) {
                                inferred_types[i] = arg_t;
                                inferred_flags[i] = 1;
                            } else {
                                if (!sem_types_are_equal(inferred_types[i], arg_t)) {
                                    if (sem_types_are_compatible(ctx, inferred_types[i], arg_t) && sem_types_are_compatible(ctx, arg_t, inferred_types[i])) {
                                        int rank_inf = get_type_rank(inferred_types[i]);
                                        int rank_arg = get_type_rank(arg_t);
                                        if (rank_arg > rank_inf) {
                                            inferred_types[i] = arg_t;
                                        }
                                    } else if (sem_types_are_compatible(ctx, inferred_types[i], arg_t)) {
                                        // Keep inferred_types[i]
                                    } else if (sem_types_are_compatible(ctx, arg_t, inferred_types[i])) {
                                        inferred_types[i] = arg_t;
                                    } else {
                                        deduction_failed = 1;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
                param = param->next;
                arg = arg->next;
            }

            for (int i = 0; i < cn->num_type_params; i++) {
                if (!inferred_flags[i]) deduction_failed = 1;
            }

            if (!deduction_failed) {
                TemplateInstNode *ti = arena_alloc_type(ctx->compiler_ctx->arena, TemplateInstNode);
                ti->base.type = NODE_TEMPLATE_INSTANTIATION;
                ti->base.line = node->base.line;
                ti->base.col = node->base.col;
                ti->target = node->target;
                ti->num_template_types = cn->num_type_params;
                ti->template_types = arena_alloc(ctx->compiler_ctx->arena, sizeof(VarType) * ti->num_template_types);

                for (int i = 0; i < cn->num_type_params; i++) {
                    ti->template_types[i] = inferred_types[i];
                }

                node->target = (ASTNode*)ti;

                // Evaluate the template instantiation
                sem_check_expr(ctx, node->target);

                if (node->target->type == NODE_TEMPLATE_INSTANTIATION) {
                    TemplateInstNode *evaluated_ti = (TemplateInstNode*)node->target;
                    if (evaluated_ti->target && evaluated_ti->target->type == NODE_VAR_REF) {
                        node->name = ((VarRefNode*)evaluated_ti->target)->name;
                    } else if (evaluated_ti->target && evaluated_ti->target->type == NODE_MEMBER_ACCESS) {
                        MethodCallNode *mc = (MethodCallNode*)node;
                        MemberAccessNode *ma = (MemberAccessNode*)evaluated_ti->target;
                        mc->base.type = NODE_METHOD_CALL;
                        mc->object = ma->object;
                        mc->method_name = ma->member_name; // the mangled name
                        mc->mangled_name = NULL;
                        mc->owner_class = NULL;
                        mc->is_static = 0;
                        sem_check_method_call(ctx, mc);
                        return;
                    }
                } else if (node->target->type == NODE_VAR_REF) {
                    node->name = ((VarRefNode*)node->target)->name;
                }

                sym = sem_symbol_lookup(ctx, node->name, NULL);
                if (sym) {
                    if (sym->kind == SYM_FUNC) {
                        SemSymbol *resolved = sem_resolve_overload(ctx, &node->args, NULL, sym, (ASTNode*)node);
                        if (resolved) {
                            node->mangled_name = resolved->mangled_name;
                            sym = resolved;
                            if (node->target && node->target->type == NODE_VAR_REF) {
                                ((VarRefNode*)node->target)->mangled_name = resolved->mangled_name;
                                sem_set_node_type(ctx, node->target, resolved->type);
                            }
                        }
                        sem_set_node_type(ctx, (ASTNode*)node, sym->type);

                        if (sym->kind == SYM_FUNC) {
                            if (!sym->is_pristine) sem_set_node_tainted(ctx, (ASTNode*)node, 1);
                        }
                        return;
                    }
                }
            }
        }

        char expected_types[256] = "";
        size_t pos = 0;
        for (int i=0; i<cn->num_type_params; i++) {
            pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, "%s", cn->type_params[i]);
            if (i < cn->num_type_params - 1 && pos < sizeof(expected_types) - 1) {
                pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, ", ");
            }
        }
        sem_error(ctx, (ASTNode*)node, "Template '%s' needs types [%s]. Type inference failed.", sym->name, expected_types);
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    if (sym->kind == SYM_CLASS) {
        VarType instance = {TYPE_CLASS, 0, arena_strdup(ctx->compiler_ctx->arena, node->name), 0, 0, NULL, NULL, 0, 0, 0, 0};
        sem_set_node_type(ctx, (ASTNode*)node, instance);

        // Find constructor
        int ctor_found = 0;
        SemSymbol *constructor_head = NULL;
        if (sym->inner_scope) {
            SemSymbol *s = sym->inner_scope->symbols;
            while (s) {
                if (streq_lit(s->name, sym->name) || streq_lit(s->name, "init")) {
                     constructor_head = s;
                     break;
                }
                s = s->next;
            }
        }

        if (constructor_head) {
            SemSymbol *resolved = sem_resolve_overload(ctx, &node->args, NULL, constructor_head, (ASTNode*)node);
            if (resolved) {
                node->mangled_name = resolved->mangled_name;
                ctor_found = 1;
            } else {
                // sem_resolve_overload emits an error if it fails
                ctor_found = 1; // Mark as found so we don't try implicit fallback
            }
        }

        // If no explicit constructor was found, it might have an implicit one generated by ALIR
        // But since we haven't run ALIR yet, we might need to simulate it or trust that it'll be checked later.
        // Actually, I added logic to ALIR to register it in the symtable, but ALIR runs AFTER semantic.
        // So we need to handle implicit constructor argument counting here too.
        if (!ctor_found && sym->trait_count > 0) {
            for (int i = 0; i < sym->trait_count; i++) {
                SemSymbol *trait_sym = sem_symbol_lookup(ctx, sym->traits[i], NULL);
                if (trait_sym && trait_sym->inner_scope) {
                    SemSymbol *s = trait_sym->inner_scope->symbols;
                    while (s) {
                        if (streq_lit(s->name, trait_sym->name) || streq_lit(s->name, "init")) {
                            constructor_head = s;
                            break;
                        }
                        s = s->next;
                    }
                    if (constructor_head) {
                        SemSymbol *resolved = sem_resolve_overload(ctx, &node->args, NULL, constructor_head, (ASTNode*)node);
                        if (resolved) {
                            node->mangled_name = resolved->mangled_name;
                            ctor_found = 1;
                            sem_warning(ctx, (ASTNode*)node, "no initialization from %s, using %s as a constructor", sym->name, trait_sym->name);
                            break;
                        }
                    }
                }
                if (ctor_found) break;
            }
        }

        if (!ctor_found) {
             if (sym->trait_count > 0) {
                 sem_warning(ctx, (ASTNode*)node, "no initialization from %s, using %s as a constructor", sym->name, sym->traits[0]);
             }
             // Basic validation for implicit constructor: should match number of fields
             int total_fields = sem_count_class_fields(ctx, sym);
             int required_fields = sem_count_required_class_fields(ctx, sym);
             int arg_count = 0;
             ASTNode *a = node->args;
             while(a) {
                 sem_check_expr(ctx, a);
                 a = a->next;
                 arg_count++;
             }
             if (arg_count < required_fields || arg_count > total_fields) {
                 int is_copy = 0;
                 if (arg_count == 1) {
                     VarType arg_type = sem_get_node_type(ctx, node->args);
                     debug_semantic("copy check: base=%d expected=%d class_name=%s sym_name=%s\n", arg_type.base, TYPE_CLASS, arg_type.class_name ? arg_type.class_name : "(null)", sym->name);
                     if (arg_type.base == TYPE_CLASS && arg_type.class_name && streq_lit(arg_type.class_name, sym->name)) {
                         is_copy = 1;
                     }
                 }
                 if (!is_copy) {
                     if (required_fields == total_fields) {
                         sem_error(ctx, (ASTNode*)node, "Expected %d argument(s) for implicit constructor of '%s', got %d", total_fields, sym->name, arg_count);
                     } else {
                         sem_error(ctx, (ASTNode*)node, "Expected between %d and %d argument(s) for implicit constructor of '%s', got %d", required_fields, total_fields, sym->name, arg_count);
                     }
                 }
             } else if (arg_count < total_fields) {
                 sem_inject_default_class_args(ctx, node, sym, arg_count, total_fields);
             }
        }
    }
 else if (sym->kind == SYM_FUNC && sym->is_flux) {

        // Rewrite flux generator return type dynamically for iterators to intercept!
        char buf[512];
        snprintf(buf, sizeof(buf), "FluxCtx_%s", sym->mangled_name ? sym->mangled_name : sym->name);
        VarType flux_type = {TYPE_CLASS, 0, arena_strdup(ctx->compiler_ctx->arena, buf), 0, 0, NULL, NULL, 0, 0, 0, 0};
        flux_type.fp_ret_type = arena_alloc_type(ctx->compiler_ctx->arena, VarType);
        *flux_type.fp_ret_type = sym->type; // Save underlying yield type
        sem_set_node_type(ctx, (ASTNode*)node, flux_type);
    } else if (sym->kind == SYM_VAR && sym->type.is_func_ptr) {
        if (sym->type.fp_ret_type) {
            sem_set_node_type(ctx, (ASTNode*)node, *sym->type.fp_ret_type);
        } else {
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        }
    } else {
        sem_set_node_type(ctx, (ASTNode*)node, sym->type);
    }
}
