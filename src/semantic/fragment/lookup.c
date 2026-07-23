#include "semantic.h"

void sem_lookup_class_call(SemanticCtx *ctx, MethodCallNode *node) {
    VarType obj_type = sem_get_node_type(ctx, node->object);

    SemSymbol *class_sym = sem_symbol_lookup(ctx, obj_type.class_name, NULL);
// printf("DEBUG: sem_lookup_class_call for '%s', class_sym=%p\n", obj_type.class_name, class_sym);
// if (class_sym) printf("DEBUG: class_sym->kind=%d\n", class_sym->kind);
    if (!class_sym || class_sym->kind != SYM_CLASS) {
        if (class_sym && class_sym->kind == SYM_TEMPLATE) {
            CompoundNode *cn = class_sym->template_node;
            char expected_types[256] = "";
            for (int i=0; i<cn->num_type_params; i++) {
                strcat(expected_types, cn->type_params[i]);
                if (i < cn->num_type_params - 1) strcat(expected_types, ", ");
            }
            sem_error(ctx, (ASTNode*)node, "'%s' needs types [%s]", obj_type.class_name, expected_types);
        } else {
            sem_error(ctx, (ASTNode*)node, "Type '%s' is not a class/struct", obj_type.class_name);
        }
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
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
        if (current_class->inner_scope) {
            SemSymbol *member = NULL;
            if (current_class->inner_scope->symbol_map) {
                member = hashmap_get((HashMap*)current_class->inner_scope->symbol_map, node->method_name);
            } else {
                member = current_class->inner_scope->symbols;
                while (member && strcmp(member->name, node->method_name) != 0) {
                    member = member->next;
                }
            }
            if (member) {

                    
                    if (ctx->current_func_sym && ctx->current_func_sym->is_pure) {
                        if (member->kind == SYM_FUNC && !member->is_pure) {
                            if (ctx->current_func_sym->must_pure) sem_error(ctx, (ASTNode*)node, "Pure function '%s' cannot call impure method '%s'", ctx->current_func_sym->name, member->name);
                            ctx->current_func_sym->is_pure = false;
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
                                if (strcmp(actual_class_name, current_class->name) != 0) {
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
                        goto done_method_search;
                    }
            }
        }
            if (current_class->trait_count > 0) {
                for (int i = 0; i < current_class->trait_count; i++) {
                    SemSymbol *trait_sym = sem_symbol_lookup(ctx, current_class->traits[i], NULL);
                    if (trait_sym && trait_sym->inner_scope) {
                        SemSymbol *member = trait_sym->inner_scope->symbols;
                        while (member) {
                            if (strcmp(member->name, node->method_name) == 0) {
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
                                                if (strcmp(vr->name, trait_sym->name) == 0) {
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
                                    goto done_method_search;
                                }
                            }
                            member = member->next;
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
    if (!found) {
         sem_error(ctx, (ASTNode*)node, "Method '%s' not found in class '%s'", node->method_name, obj_type.class_name);
         sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    }
}
