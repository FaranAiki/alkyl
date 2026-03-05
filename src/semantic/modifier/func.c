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

    if (obj_type.base == TYPE_CLASS && obj_type.class_name) {
        sem_lookup_class_call(ctx, node);
    } else if (obj_type.base == TYPE_NAMESPACE && obj_type.class_name) {
        SemSymbol *ns_sym = sem_symbol_lookup(ctx, obj_type.class_name, NULL);
        if (!ns_sym || ns_sym->kind != SYM_NAMESPACE) {
            sem_error(ctx, (ASTNode*)node, "'%s' is not a namespace", obj_type.class_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            return;
        }

        int found = 0;
        if (ns_sym->inner_scope) {
            SemSymbol *member = ns_sym->inner_scope->symbols;
            while (member) {
                if (strcmp(member->name, node->method_name) == 0) {
                    if (ctx->current_func_sym && ctx->current_func_sym->is_pure) {
                        if (member->kind == SYM_FUNC && !member->is_pure) {
                            sem_error(ctx, (ASTNode*)node, "Pure function '%s' cannot call impure method '%s'", ctx->current_func_sym->name, member->name);
                        }
                    }

                    if (member->kind == SYM_FUNC && !member->is_pristine) {
                        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
                    }

                    if (member->kind == SYM_FUNC) {
                        if (member->is_flux) {
                            char buf[256];
                            snprintf(buf, sizeof(buf), "FluxCtx_%s_%s", ns_sym->name, member->name);
                            VarType flux_type = {TYPE_CLASS, 1, 0, arena_strdup(ctx->compiler_ctx->arena, buf), 0, 0, NULL, NULL, 0, 0, 0, 0};
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

                    if (found) {
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
                        goto done_ns_method_search;
                    }
                }
                member = member->next;
            }
        }
        
        done_ns_method_search:
        if (!found) {
             sem_error(ctx, (ASTNode*)node, "Function '%s' not found in namespace '%s'", node->method_name, obj_type.class_name);
             sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        }
    } else {
        sem_error(ctx, (ASTNode*)node, "Cannot call method on non-class/non-namespace type");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    }
}

void sem_check_func_def(SemanticCtx *ctx, FuncDefNode *node) {
    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return;

    sem_scope_enter(ctx, 1, node->ret_type);
    
    SemSymbol *old_func = ctx->current_func_sym;
    ctx->current_func_sym = sem_symbol_lookup(ctx, node->name, NULL);

    if (node->class_name) {
        VarType this_type = {TYPE_CLASS, 1, 0, arena_strdup(ctx->compiler_ctx->arena, node->class_name), 0, 0, NULL, NULL, 0, 0, 0, 0}; 
        sem_symbol_add(ctx, "this", SYM_VAR, this_type);
    }

    Parameter *p = node->params;
    while (p) {
        if (p->name) {
            SemSymbol *s = sem_symbol_add(ctx, p->name, SYM_VAR, p->type);
            s->is_initialized = 1;
        }
        p = p->next;
    }
    
    sem_check_block(ctx, node->body);
    sem_scope_exit(ctx);
    
    ctx->current_func_sym = old_func;
}
