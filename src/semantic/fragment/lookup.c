#include "semantic.h"

int sem_lookup_class_call(SemanticCtx *ctx, MethodCallNode *node) {
    VarType obj_type = sem_get_node_type(ctx, node->object);

    SemSymbol *class_sym = sem_symbol_lookup_type(ctx, obj_type.class_name);
    debug_semantic("sem_lookup_class_call for '%s', class_sym=%p\n", obj_type.class_name, class_sym);
    if (!class_sym || class_sym->kind != SYM_CLASS) {
        if (class_sym) { debug_semantic("'%s' kind is %d\n", obj_type.class_name, class_sym->kind); }
        if (class_sym && class_sym->kind == SYM_TEMPLATE) {
            CompoundNode *cn = class_sym->template_node;
            char expected_types[256] = "";
            size_t pos = 0;
            for (int i=0; i<cn->num_type_params; i++) {
                pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, "%s", cn->type_params[i]);
                if (i < cn->num_type_params - 1 && pos < sizeof(expected_types) - 1) {
                    pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, ", ");
                }
            }
        }
        return 0;
    }

    SemSymbol *current_class = class_sym;
    char *actual_class_name = class_sym->name;
    if (node->object && node->object->type == NODE_INDEX_ACCESS) {
        IndexAccessNode *aa = (IndexAccessNode*)node->object;
        VarType base_type = sem_get_node_type(ctx, aa->target);
        if (base_type.base == TYPE_CLASS && base_type.class_name) {
            actual_class_name = base_type.class_name;
        }
    }

    int found = 0;
    // TODO fix this parsing for current_class!
    while (current_class) {
        if (current_class->inner_scope && current_class->inner_scope->symbol_map) {
            SemSymbol *member = hashmap_get((HashMap*)current_class->inner_scope->symbol_map, node->method_name);
            if (member) {
                    if (ctx->current_func_sym && ctx->current_func_sym->is_pure) {
                        if (member->kind == SYM_FUNC && !member->is_pure) {
                            if (ctx->current_func_sym->must_pure) sem_error(ctx, (ASTNode*)node, "Pure function '%s' cannot call impure method '%s'", ctx->current_func_sym->name, member->name);
                            ctx->current_func_sym->is_pure = false;
                        }
                    }

                    if (ctx->current_func_sym && ctx->current_func_sym->is_total) {
                        if (member->kind == SYM_FUNC && !member->is_total) {
                            if (ctx->current_func_sym->must_total) sem_error(ctx, (ASTNode*)node, "Total function '%s' cannot call partial method '%s'", ctx->current_func_sym->name, member->name);
                            ctx->current_func_sym->is_total = false;
                        }
                    }

                    if (member->kind == SYM_FUNC && !member->is_pristine) {
                        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
                    }

                    if (member->kind == SYM_FUNC) {
                        if (member->is_flux) {
                            char buf[256];
                            snprintf(buf, sizeof(buf), "FluxCtx_%s_%s", current_class->name, member->name);
                            VarType flux_type = {TYPE_CLASS, 1, arena_strdup(ctx->compiler_ctx->arena, buf), 0, 0, NULL, NULL, 0, 0, 0, 0};
                            flux_type.fp_ret_type = arena_alloc_type(ctx->compiler_ctx->arena, VarType);
                            *flux_type.fp_ret_type = member->type; // Bind the underlying yield type natively!
                            sem_set_node_type(ctx, (ASTNode*)node, flux_type);
                        } else {
                            sem_set_node_type(ctx, (ASTNode*)node, member->type);
                        }
                        node->owner_class = current_class->name;
                        found = 1;
                    }
                    else if (member->kind == SYM_VAR && member->type.is_func_ptr) {
                         sem_set_node_type(ctx, (ASTNode*)node, *member->type.fp_ret_type);
                         found = 1;
                    }

                    if (found) {
                        if (member->kind == SYM_FUNC) {
                            SemSymbol *resolved = sem_resolve_overload(ctx, &node->args, NULL, member, (ASTNode*)node);
                            if (resolved && resolved->mangled_name) {
                                if (!streq(actual_class_name, current_class->name)) {
                                    int prefix_len = strlen(current_class->name);
                                    if (strncmp(resolved->mangled_name, current_class->name, prefix_len) == 0 && resolved->mangled_name[prefix_len] == '_') {
                                        char buf[512];
                                        snprintf(buf, sizeof(buf), "%s%s", actual_class_name, resolved->mangled_name + prefix_len);
                                        node->mangled_name = arena_strdup(ctx->compiler_ctx->arena, buf);
                                    } else {
                                        node->mangled_name = resolved->mangled_name;
                                    }
                                } else {
                                    node->mangled_name = resolved->mangled_name;
                                }
                            }
                        } else {
                            // Var func ptr call etc
                            ASTNode **curr_arg = &node->args;
                            while(*curr_arg) {
                                sem_check_expr(ctx, *curr_arg);
                                curr_arg = &(*curr_arg)->next;
                            }
                        }
                        return 1;
                    }
            }
        }
            if (current_class->trait_count > 0) {
                for (int i = 0; i < current_class->trait_count; i++) {
                    SemSymbol *trait_sym = sem_symbol_lookup(ctx, current_class->traits[i], NULL);
                    if (trait_sym && trait_sym->inner_scope && trait_sym->inner_scope->symbol_map) {
                        SemSymbol *member = hashmap_get((HashMap*)trait_sym->inner_scope->symbol_map, node->method_name);
                        if (member) {
                            if (member->kind == SYM_FUNC) {
                                sem_set_node_type(ctx, (ASTNode*)node, member->type);
                                node->owner_class = current_class->name; // or trait_sym->name? Let's use current_class for inheritance flattening
                                found = 1;
                            } else if (member->kind == SYM_VAR && member->type.is_func_ptr) {
                                sem_set_node_type(ctx, (ASTNode*)node, *member->type.fp_ret_type);
                                found = 1;
                            }
                            if (found) {
                                char *obj_name = "obj";
                                int should_warn = 1;
                                if (node->object) {
                                    if (node->object->type == NODE_VAR_REF) {
                                        obj_name = ((VarRefNode*)node->object)->name;
                                    } else if (node->object->type == NODE_INDEX_ACCESS) {
                                        IndexAccessNode *aa = (IndexAccessNode*)node->object;
                                        if (aa->index->type == NODE_VAR_REF) {
                                            VarRefNode *vr = (VarRefNode*)aa->index;
                                            if (streq(vr->name, trait_sym->name)) {
                                                should_warn = 0; // Explicitly qualified
                                            }
                                        }
                                    }
                                }
                                if (should_warn) {
                                    sem_warning(ctx, (ASTNode*)node, "%s is from %s, consider %s[%s].%s", node->method_name, trait_sym->name, obj_name, trait_sym->name, node->method_name);
                                }
                                int arg_count = 0;
                                ASTNode **curr_arg = &node->args;
                                while(*curr_arg) {
                                    sem_check_expr(ctx, *curr_arg);

                                    if (member->kind == SYM_FUNC && member->params && arg_count < member->param_count) {
                                        sem_insert_implicit_cast(ctx, curr_arg, member->params[arg_count].type);
                                    }

                                    curr_arg = &(*curr_arg)->next;
                                    arg_count++;
                                }
                                if (member->kind == SYM_FUNC) {
                                    SemSymbol *resolved = sem_resolve_overload(ctx, &node->args, NULL, member, (ASTNode*)node);
                                    if (resolved && resolved->mangled_name) {
                                        int prefix_len = strlen(trait_sym->name);
                                        if (strncmp(resolved->mangled_name, trait_sym->name, prefix_len) == 0 && resolved->mangled_name[prefix_len] == '_') {
                                            char buf[512];
                                            snprintf(buf, sizeof(buf), "%s%s", actual_class_name, resolved->mangled_name + prefix_len);
                                            node->mangled_name = arena_strdup(ctx->compiler_ctx->arena, buf);
                                        } else {
                                            node->mangled_name = resolved->mangled_name;
                                        }
                                    }
                                }
                                return 1;
                            }
                        }
                    }
                }
            }
            if (current_class->parent_name) {
                current_class = sem_symbol_lookup(ctx, current_class->parent_name, NULL);
            } else {
                current_class = NULL;
            }
        }


    done_method_search:
    return 0;
}

SemSymbol* sem_resolve_overload(SemanticCtx *ctx, ASTNode **args, int *out_arg_count, SemSymbol *first_sym, ASTNode *err_node) {
    int arg_count = 0;
    ASTNode *curr_arg = *args;
    while(curr_arg) {
        sem_check_expr(ctx, curr_arg);
        curr_arg = curr_arg->next;
        arg_count++;
    }
    if (out_arg_count) *out_arg_count = arg_count;

    SemSymbol *sym = first_sym;
    SemSymbol *best_match = NULL;
    int best_score = -1;

    ASTNode **best_matched_args = NULL;
    ASTNode *best_varargs_head = NULL;

    // Find matching overload (exact types or compatible implicit cast)
    while (sym) {
        if (sym->param_count <= arg_count || sym->is_variadic || 1) { // 1 because of default args
            int match = 1;
            int exact_matches = 0;

            ASTNode **matched_args = arena_alloc(ctx->compiler_ctx->arena, sizeof(ASTNode*) * (sym->param_count > 0 ? sym->param_count : 1));
            for (int i=0; i<sym->param_count; i++) matched_args[i] = NULL;

            ASTNode *varargs_head = NULL;
            ASTNode **curr_vararg = &varargs_head;

            int pos_idx = 0;
            curr_arg = *args;
            while(curr_arg) {
                if (curr_arg->type == NODE_NAMED_ARG) {
                    NamedArgNode *narg = (NamedArgNode*)curr_arg;
                    int found = -1;
                    Parameter *p = sym->params;
                    for (int i=0; p; i++, p=p->next) {
                        if (p->name && streq(p->name, narg->name)) { found = i; break; }
                    }
                    if (found == -1 || matched_args[found] != NULL) { match = 0; break; }
                    matched_args[found] = narg->value;
                } else {
                    if (pos_idx < sym->param_count) {
                        if (matched_args[pos_idx] != NULL) { match = 0; break; }
                        matched_args[pos_idx] = curr_arg;
                        pos_idx++;
                    } else if (sym->is_variadic) {
                        *curr_vararg = curr_arg;
                        curr_vararg = &(*curr_vararg)->next;
                    } else {
                        match = 0; break;
                    }
                }
                curr_arg = curr_arg->next;
            }

            if (match) {
                Parameter *p = sym->params;
                for (int i=0; i<sym->param_count; i++, p=p->next) {
                    if (matched_args[i] == NULL) {
                        if (p->default_value) {
                            matched_args[i] = p->default_value;
                        } else {
                            match = 0; break;
                        }
                    }
                    sem_check_expr(ctx, matched_args[i]);
                    VarType arg_t = sem_get_node_type(ctx, matched_args[i]);
                    if (!sem_types_are_compatible(ctx, p->type, arg_t)) { match = 0; break; }
                    if (p->type.base == arg_t.base && p->type.ptr_depth == arg_t.ptr_depth) exact_matches++;
                }
            }

            if (match) {
                int score = exact_matches;
                if (score > best_score) {
                    best_score = score;
                    best_match = sym;
                    best_matched_args = matched_args;
                    if (*curr_vararg) *curr_vararg = NULL; // terminate varargs list safely
                    best_varargs_head = varargs_head;
                }
            }
        }
        sym = sym->overload_next;
    }

    if (!best_match) {
        if (err_node) {
            StringBuilder sb;
            sb_init(&sb, ctx->compiler_ctx->arena);
            ASTNode *curr = *args;
            int first = 1;
            while(curr) {
                if (!first) sb_append(&sb, ", ");
                VarType arg_t = sem_get_node_type(ctx, curr);
                sb_append(&sb, sem_type_to_str(arg_t));
                first = 0;
                curr = curr->next;
            }
            sem_error(ctx, err_node, "No matching overload found for function '%s(%s)'", first_sym->name, sb_return(&sb));
        }
        return NULL;
    }

    // Rebuild arguments list
    if (best_matched_args) {
        ASTNode *new_args_head = NULL;
        ASTNode **curr_new = &new_args_head;
        for (int i=0; i<best_match->param_count; i++) {
            *curr_new = best_matched_args[i];
            curr_new = &(*curr_new)->next;
        }
        if (best_varargs_head) {
            *curr_new = best_varargs_head;
        } else {
            *curr_new = NULL;
        }
        *args = new_args_head;
    }

    // Apply implicit casts and reference downgrades
    ASTNode **p_curr = args;
    Parameter *curr_para = best_match->params;

    ASTNode *arg_node = *args;
    while(arg_node) {
        if (!best_match->is_pristine && arg_node->type == NODE_UNARY_OP) {
            UnaryOpNode *uop = (UnaryOpNode*)arg_node;
            if (uop->op == TOKEN_AND && uop->operand->type == NODE_VAR_REF) {
                VarRefNode *var_ref = (VarRefNode*)uop->operand;
                SemSymbol *ref_sym = sem_symbol_lookup(ctx, var_ref->name, NULL);
                if (ref_sym) {
                    if (ref_sym->must_pristine) {
                        sem_error(ctx, arg_node, "Cannot pass pristine variable '%s' by reference to tainted function '%s'", var_ref->name, best_match->name);
                    } else {
                        ref_sym->is_pristine = 0; // Downgrade to tainted
                    }
                }
            }
        }
        arg_node = arg_node->next;
    }

    while(*p_curr && curr_para) {
        int arg_is_tainted = sem_get_node_tainted(ctx, *p_curr);
        int param_is_pristine = curr_para->is_pristine;

        if (arg_is_tainted && param_is_pristine) {
            sem_error(ctx, *p_curr, "Cannot pass tainted expression to pristine parameter '%s'", curr_para->name);
            sem_hint(ctx, *p_curr, "Expressions containing division by a non-constant can lead to division by error");
        }

        if (sem_types_are_compatible(ctx, curr_para->type, sem_get_node_type(ctx, *p_curr))) {
            sem_insert_implicit_cast(ctx, p_curr, curr_para->type);
        }
        p_curr = &(*p_curr)->next;
        curr_para = curr_para->next;
    }

    if (best_match->is_variadic) {
        while (*p_curr) {
            if (sem_get_node_tainted(ctx, *p_curr)) {
                sem_error(ctx, *p_curr, "Cannot pass tainted expression to varargs (...) of function '%s'", best_match->name);
                sem_hint(ctx, *p_curr, "Expressions containing division by a non-constant can lead to division by error");
            }
            p_curr = &(*p_curr)->next;
        }
    }

    return best_match;
}
