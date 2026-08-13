#include "semantic.h"

void sem_check_stmt(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return;
    if (node->filename) ctx->current_filename = node->filename;
    if (node->is_macro_arg) return;
    ctx->current_node = node;

    switch (node->type) {
        case NODE_PURGE: {
            PurgeNode *pn = (PurgeNode*)node;

            if (!pn->target && ctx->current_func_sym) {
                if (ctx->current_func_sym->has_explicit_pristine) {
                    sem_error(ctx, node, "Pristine function '%s' cannot use purge without a target", ctx->current_func_sym->name);
                } else if (!ctx->current_func_sym->type.is_tainted) {
                    sem_hint(ctx, node, "Function '%s' uses purge without a target, consider marking it as tainted", ctx->current_func_sym->name);
                }
            }

            if (pn->msg->type == NODE_VAR_REF) {
                VarRefNode *var = (VarRefNode*)pn->msg;
                // It's an error identifier!
                if (!hashmap_get(&ctx->compiler_ctx->error_table, var->name)) {
                    int id = ctx->compiler_ctx->next_error_id++;
                    hashmap_put(&ctx->compiler_ctx->error_table, var->name, (void*)(intptr_t)(id + 1));
                }
                // No further type check on var because it's just an error identifier
            } else {
                sem_error(ctx, node, "purge requires an error identifier (e.g., ErrDivisionByZero)");
            }

            if (pn->target) {
                sem_check_expr(ctx, pn->target);
                VarType target_type = sem_get_node_type(ctx, pn->target);
                if (!target_type.is_tainted) {
                    sem_error(ctx, pn->target, "Cannot purge into a pristine or non-tainted variable");
                }
                if (pn->target->type != NODE_VAR_REF && pn->target->type != NODE_MEMBER_ACCESS && pn->target->type != NODE_INDEX_ACCESS) {
                    sem_error(ctx, pn->target, "Target of purge must be a mutable lvalue");
                }
            }
            break;
        }
        case NODE_VAR_DECL: sem_check_var_decl(ctx, (VarDeclNode*)node, 1); break;
        case NODE_ASSIGN: sem_check_assign(ctx, (AssignNode*)node); break;
        case NODE_RETURN: {
            ReturnNode *rn = (ReturnNode*)node;
            if (rn->value) {
                sem_check_expr(ctx, rn->value);
                VarType val = sem_get_node_type(ctx, rn->value);

                if (ctx->current_func_sym && ctx->current_func_sym->must_pristine && sem_get_node_tainted(ctx, rn->value)) {
                    sem_error(ctx, node, "Pristine function '%s' cannot return a tainted value", ctx->current_func_sym->name);
                }

                if (ctx->current_scope->is_function_scope) {
                    if (!sem_types_are_compatible(ctx,ctx->current_scope->expected_ret_type, val)) {
                        debug_semantic("mismatch: expected base=%d ptr=%d, got base=%d ptr=%d\n", ctx->current_scope->expected_ret_type.base, ctx->current_scope->expected_ret_type.ptr_depth, val.base, val.ptr_depth); sem_error(ctx, node, "Return type mismatch");
                    } else {
                         sem_insert_implicit_cast(ctx, &rn->value, ctx->current_scope->expected_ret_type);
                    }
                }
            } else {
                 if (ctx->current_scope->is_function_scope && ctx->current_scope->expected_ret_type.base != TYPE_VOID) {
                     sem_error(ctx, node, "Function must return a value");
                 }
            }
            break;
        }
        case NODE_CLEAN: {
            CleanNode *cn = (CleanNode*)node;
            SemSymbol *target_sym = sem_symbol_lookup(ctx, cn->var_name, NULL);
            if (!target_sym) {
                sem_error(ctx, node, "Unknown variable '%s' in clean statement", cn->var_name);
                break;
            }
            if (!target_sym->type.is_tainted) {
                sem_error(ctx, node, "Variable '%s' is not tainted", cn->var_name);
            }

            sem_scope_enter(ctx, 0, (VarType){0});
            VarType pristine_type = target_sym->type;
            pristine_type.is_tainted = 0;
            const char *target_name = cn->pristine_var_name ? cn->pristine_var_name : cn->var_name;
            SemSymbol *pristine_sym = sem_symbol_add(ctx, target_name, SYM_VAR, pristine_type);
            pristine_sym->is_initialized = 1;
            pristine_sym->is_pristine = 1;

            sem_check_block(ctx, cn->body);
            sem_scope_exit(ctx);

            if (cn->residue_cases || cn->residue_body) {
                sem_scope_enter(ctx, 0, (VarType){0});
                VarType err_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
                SemSymbol *err_sym = sem_symbol_add(ctx, cn->err_var_name, SYM_VAR, err_type);
                err_sym->is_initialized = 1;

                int has_default = 0;
                for (ResidueCase *rc = cn->residue_cases; rc; rc = rc->next) {
                    if (rc->is_default) has_default = 1;
                    sem_check_block(ctx, rc->body);
                }
                if (cn->residue_body) sem_check_block(ctx, cn->residue_body);
                sem_scope_exit(ctx);

                if (target_sym->has_errnum) {
                    sem_check_residue_exhaustive(ctx, node, target_sym, cn->residue_cases, has_default);
                }
            }
            break;
        }
        case NODE_UNTAINT: {
            UntaintNode *un = (UntaintNode*)node;
            SemSymbol *target_sym = sem_symbol_lookup(ctx, un->var_name, NULL);
            if (!target_sym) {
                sem_error(ctx, node, "Unknown variable '%s' in untaint statement", un->var_name);
                break;
            }
            if (!target_sym->type.is_tainted) {
                sem_error(ctx, node, "Variable '%s' is not tainted", un->var_name);
            }

            if (un->residue_cases || un->residue_body) {
                sem_scope_enter(ctx, 0, (VarType){0});
                VarType err_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
                SemSymbol *err_sym = sem_symbol_add(ctx, un->err_var_name, SYM_VAR, err_type);
                err_sym->is_initialized = 1;

                int has_default = 0;
                for (ResidueCase *rc = un->residue_cases; rc; rc = rc->next) {
                    if (rc->is_default) has_default = 1;
                    sem_check_block(ctx, rc->body);
                }
                if (un->residue_body) sem_check_block(ctx, un->residue_body);
                sem_scope_exit(ctx);

                if (target_sym->has_errnum) {
                    sem_check_residue_exhaustive(ctx, node, target_sym, un->residue_cases, has_default);
                }
            }

            target_sym->is_pristine = 1;
            target_sym->type.is_tainted = 0;
            break;
        }
        case NODE_ERRNUM:
            break;
        case NODE_IF: {
            IfNode *ifn = (IfNode*)node;
            sem_check_expr(ctx, ifn->condition);
            if (sem_get_node_type(ctx, ifn->condition).is_tainted) {
                sem_error(ctx, ifn->condition, "Condition is tainted");
                sem_emit_fallback_hint(ctx, ifn->condition);
            }

            int cond_val = -1;
            if (ifn->condition->type == NODE_BINARY_OP) {
                BinaryOpNode *bin = (BinaryOpNode*)ifn->condition;
                if (bin->op == TOKEN_EQ || bin->op == TOKEN_NEQ) {
                    ASTNode *l = bin->left;
                    ASTNode *r = bin->right;
                    if (l && l->type == NODE_CAST) l = ((CastNode*)l)->operand;
                    if (l && l->type == NODE_BEING) l = ((BeingNode*)l)->operand;
                    if (r && r->type == NODE_CAST) r = ((CastNode*)r)->operand;
                    if (r && r->type == NODE_BEING) r = ((BeingNode*)r)->operand;

                    if (l && r && l->type == NODE_TYPEOF && r->type == NODE_TYPEOF) {
                        SizeOfNode *sl = (SizeOfNode*)l;
                        SizeOfNode *sr = (SizeOfNode*)r;
                        VarType tl = sl->target_type.base != TYPE_UNKNOWN ? sl->target_type : sem_get_node_type(ctx, sl->operand);
                        VarType tr = sr->target_type.base != TYPE_UNKNOWN ? sr->target_type : sem_get_node_type(ctx, sr->operand);
                        cond_val = (sem_types_are_equal(tl, tr) || sem_types_are_compatible(ctx, tl, tr)) ? 1 : 0;
                        if (bin->op == TOKEN_NEQ) cond_val = 1 - cond_val;
                    }
                }
            }

            if (cond_val == -1 || cond_val == 1) {
                sem_scope_enter(ctx, 0, (VarType){0});
                sem_check_block(ctx, ifn->then_body);
                sem_scope_exit(ctx);
            }

            if (ifn->else_body && (cond_val == -1 || cond_val == 0)) {
                sem_scope_enter(ctx, 0, (VarType){0});
                if (ifn->else_body->type == NODE_IF) {
                    sem_check_node(ctx, ifn->else_body);
                } else {
                    sem_check_block(ctx, ifn->else_body);
                }
                sem_scope_exit(ctx);
            }
            break;
        }
        case NODE_SWITCH: {
            SwitchNode *sn = (SwitchNode*)node;
            sem_check_expr(ctx, sn->condition);
            if (sem_get_node_type(ctx, sn->condition).is_tainted) {
                sem_error(ctx, sn->condition, "Condition is tainted");
                sem_emit_fallback_hint(ctx, sn->condition);
            }

            CaseNode *sc = (CaseNode*)sn->cases;
            while (sc) {
                if (sc->value) {
                    sem_check_expr(ctx, sc->value);
                }
                sem_scope_enter(ctx, 0, (VarType){0});
                sem_check_block(ctx, sc->body);
                sem_scope_exit(ctx);
                sc = (CaseNode*)sc->base.next;
            }
            if (sn->default_case) {
                sem_scope_enter(ctx, 0, (VarType){0});
                sem_check_block(ctx, sn->default_case);
                sem_scope_exit(ctx);
            }
            break;
        }
        case NODE_DEFER: {
            DeferNode *dn = (DeferNode*)node;
            sem_scope_enter(ctx, 0, (VarType){0});
            sem_check_block(ctx, dn->body);
            sem_scope_exit(ctx);
            break;
        }
        case NODE_WHILE: {
            WhileNode *wn = (WhileNode*)node;
            if (ctx->current_func_sym) {
                if (ctx->current_func_sym->must_total) {
                    sem_error(ctx, node, "Function '%s' is marked total but contains a while loop", ctx->current_func_sym->name);
                }
                ctx->current_func_sym->is_total = false;
            }
            sem_check_expr(ctx, wn->condition);
            if (wn->condition && sem_get_node_type(ctx, wn->condition).is_tainted) {
                sem_error(ctx, wn->condition, "Condition is tainted");
                sem_emit_fallback_hint(ctx, wn->condition);
            }
            ctx->in_loop++;

            sem_scope_enter(ctx, 0, (VarType){0});
            sem_check_block(ctx, wn->body);
            sem_scope_exit(ctx);

            ctx->in_loop--;
            break;
        }
        case NODE_LOOP: {
            LoopNode *ln = (LoopNode*)node;
            if (ctx->current_func_sym) {
                if (ctx->current_func_sym->must_total) {
                    sem_error(ctx, node, "Function '%s' is marked total but contains a loop", ctx->current_func_sym->name);
                }
                ctx->current_func_sym->is_total = false;
            }
            sem_check_expr(ctx, ln->iterations);
            if (sem_get_node_type(ctx, ln->iterations).is_tainted) {
                sem_error(ctx, ln->iterations, "Loop iterations expression is tainted");
                sem_hint(ctx, ln->iterations, "Use untaint, wash, clean, or ? operator to handle the error state");
            }
            ctx->in_loop++;

            sem_scope_enter(ctx, 0, (VarType){0});
            sem_check_block(ctx, ln->body);
            sem_scope_exit(ctx);

            ctx->in_loop--;
            break;
        }
        case NODE_FOR_IN: {
            ForInNode *fn = (ForInNode*)node;
            if (ctx->current_func_sym) {
                if (ctx->current_func_sym->must_total) {
                    sem_error(ctx, node, "Function '%s' is marked total but contains a for-in loop", ctx->current_func_sym->name);
                }
                ctx->current_func_sym->is_total = false;
            }
            sem_check_expr(ctx, fn->collection);
            if (sem_get_node_type(ctx, fn->collection).is_tainted) {
                sem_error(ctx, fn->collection, "Collection is tainted");
                sem_hint(ctx, fn->collection, "Use untaint, wash, clean, or ? operator to handle the error state");
            }
            ctx->in_loop++;
            sem_check_for_in(ctx, node);
            ctx->in_loop--;
            break;
        }
        case NODE_BREAK:
            if (ctx->in_loop == 0 && ctx->in_switch == 0) sem_error(ctx, node, "'break' outside loop or switch");
            break;
        case NODE_CONTINUE:
            if (ctx->in_loop == 0) sem_error(ctx, node, "'continue' outside loop");
            break;
        case NODE_INC_DEC:
        case NODE_CALL:
        case NODE_METHOD_CALL:
        case NODE_MEMBER_ACCESS:
        case NODE_INDEX_ACCESS:
        case NODE_BINARY_OP:
        case NODE_UNARY_OP:
        case NODE_VAR_REF:
        case NODE_LITERAL:
        case NODE_ARRAY_LIT:
        case NODE_CAST:
        case NODE_BEING:
        case NODE_TYPEOF:
        case NODE_SIZEOF:
        case NODE_ALIGNOF:
        case NODE_IMPORT_EXPR:
            sem_check_expr(ctx, node);
            break;
        case NODE_EMIT: {
            EmitNode *en = (EmitNode*)node;
            sem_check_expr(ctx, en->value);
            break;
        }
      /*
        FUNC_DEF
        SWITCH
        CASE
        VAR_REF
        BINARY_OP
        UNARY_OP
        LITERAL
        ARRAY_LIT
        ARRAY_ACCESS
        VECTOR_ACCESS
        INC_DEC
        LINK
        CLASS
        NAMESPACE
        ENUM
        MEMBER_ACCESS
        TRAIT_ACCESS
        TYPEOF
        HAS_METHOD
        HAS_ATTRIBUTE
        CAST
        CLEAN
      */
        default: break;
    }
}

void sem_check_node(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return;
    if (node->filename) ctx->current_filename = node->filename;
    if (node->type == NODE_FUNC_DEF) sem_check_func_def(ctx, (FuncDefNode*)node);
    else if (node->type == NODE_CLASS) {
        ClassNode *cn = (ClassNode*)node;
        if (cn->is_abstract && cn->is_exact) sem_error(ctx, node, "Class cannot be both abstract and exact");
        if (cn->is_method_class && cn->is_container) sem_error(ctx, node, "Class cannot be both method and container");
        SemSymbol *sym = sem_symbol_lookup_type(ctx, cn->name);
        if (cn->parent_name) {
            SemSymbol *ps = sem_symbol_lookup_type(ctx, cn->parent_name);
            if (!ps || ps->kind != SYM_CLASS) {
                sem_error(ctx, node, "Undefined parent class '%s'", cn->parent_name);
            }
        }
        for (int i = 0; i < cn->traits.count; i++) {
            SemSymbol *ts = sem_symbol_lookup_type(ctx, cn->traits.names[i]);
            if (!ts || ts->kind != SYM_CLASS) {
                sem_error(ctx, node, "Undefined composition class '%s'", cn->traits.names[i]);
            }
        }

        if (sym && sym->inner_scope) {
            SemScope *old = ctx->current_scope;
            ctx->current_scope = sym->inner_scope;

            ASTNode *mem = cn->members;
            while(mem) {
                if (mem->type == NODE_FUNC_DEF) {
                    FuncDefNode *f = (FuncDefNode*)mem;
                    if (cn->is_container) {
                        char buf[256]; snprintf(buf, sizeof(buf), "Functions are not allowed in container class '%s'", cn->name);
                        sem_error(ctx, mem, buf);
                    }
                    if (cn->is_abstract && f->body != NULL) {
                        char buf[256]; snprintf(buf, sizeof(buf), "Function '%s' cannot be implemented in abstract class '%s'", f->name, cn->name);
                        sem_error(ctx, mem, buf);
                    }
                    if (cn->is_exact && f->body == NULL) {
                        char buf[256]; snprintf(buf, sizeof(buf), "Function '%s' must be implemented in exact class '%s'", f->name, cn->name);
                        sem_error(ctx, mem, buf);
                    }
                    sem_check_func_def(ctx, f);
                }
                else if (mem->type == NODE_VAR_DECL) {
                    VarDeclNode *v = (VarDeclNode*)mem;
                    if (cn->is_method_class) {
                        char buf[256]; snprintf(buf, sizeof(buf), "Variables are not allowed in method class '%s'", cn->name);
                        sem_error(ctx, mem, buf);
                    }
                    if (cn->is_abstract && v->initializer != NULL) {
                        char buf[256]; snprintf(buf, sizeof(buf), "Variable '%s' cannot have default value in abstract class '%s'", v->name, cn->name);
                        sem_error(ctx, mem, buf);
                    }
                    if (cn->is_exact && v->initializer == NULL) {
                        char buf[256]; snprintf(buf, sizeof(buf), "Variable '%s' must have default value in exact class '%s'", v->name, cn->name);
                        sem_error(ctx, mem, buf);
                    }
                    sem_check_var_decl(ctx, v, 0);
                }
                mem = mem->next;
            }

            ctx->current_scope = old;
        }
    }
    else if (node->type == NODE_NAMESPACE) {
        NamespaceNode *ns = (NamespaceNode*)node;
        SemSymbol *sym = sem_symbol_lookup(ctx, ns->name, NULL);
        if (sym && sym->inner_scope) {
            SemScope *old = ctx->current_scope;
            ctx->current_scope = sym->inner_scope;

            const char *old_ns = arena_strdup(ctx->compiler_ctx->arena, diag_get_namespace(ctx->compiler_ctx));
            diag_set_namespace(ctx->compiler_ctx, ns->name);

            sem_check_block(ctx, ns->body);

            diag_set_namespace(ctx->compiler_ctx, old_ns);
            ctx->current_scope = old;
        }
    }
    else if (node->type == NODE_VAR_DECL) {
        int register_sym = 1;
        SemScope *s = ctx->current_scope;
        int in_func = 0;
        while (s) {
            if (s->is_function_scope) in_func = 1;
            s = s->parent;
        }
        if (!in_func) {
            register_sym = 0;
        }
        sem_check_var_decl(ctx, (VarDeclNode*)node, register_sym);
    }
    else if (node->type == NODE_COMPOUND) {
        // Skip checking uninstantiated templates
        return;
    }
    else if (node->type == NODE_IMPORT) {
        ImportNode *in = (ImportNode*)node;
        if (in->resolved_body) {
            ASTNode *curr = in->resolved_body;
            while (curr) { sem_check_node(ctx, curr); curr = curr->next; }
        }
    }
    else {
        sem_check_stmt(ctx, node);
    }
}


void sem_check_block(SemanticCtx *ctx, ASTNode *block) {
    debug_semantic("sem_check_block: ns='%s'\n", ctx->compiler_ctx ? diag_get_namespace(ctx->compiler_ctx) : "(null)");
    ASTNode *curr = block;
    while (curr) {
        debug_semantic("sem_check_block: visiting node type=%d\n", curr->type);
        if (curr->type == NODE_CALL) {
            CallNode* cn = (CallNode*) curr;
            debug_semantic("sem_check_block: Call line=%d col=%d name=%s target_type=%d node=%p\n", curr->line, curr->col, cn->name ? cn->name : "(null)", cn->target ? (int)cn->target->type : -1, (void*)curr);
        }
        sem_check_node(ctx, curr);
        curr = curr->next;
    }
}
