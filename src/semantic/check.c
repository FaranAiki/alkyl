#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

SemSymbol* lookup_local_symbol(SemanticCtx *ctx, const char *name) {
    if (!ctx->current_scope) return NULL;
    SemSymbol *sym = ctx->current_scope->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) return sym;
        sym = sym->next;
    }
    return NULL;
}

// If `node` is a call to a function that has an attached `errnum [...]` set,
// returns that function's SemSymbol (so the caller can enumerate its errors).
// Otherwise returns NULL.
SemSymbol* sem_get_errnum_func_sym(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return NULL;
    // Unwrap assignment RHS etc. by inspecting the inner call expression.
    if (node->type == NODE_CALL) {
        CallNode *cn = (CallNode*)node;
        char *fname = cn->name;
        if (!fname && cn->target && cn->target->type == NODE_MEMBER_ACCESS) {
            fname = ((MemberAccessNode*)cn->target)->member_name;
        }
        if (fname) {
            SemSymbol *sym = sem_symbol_lookup(ctx, fname, NULL);
            if (sym && sym->kind == SYM_FUNC && sym->has_errnum) return sym;
        }
    }
    return NULL;
}

// Checks that the residue cases cover every error in `err_sym`'s error set.
// `default_case` indicates whether a catch-all default is present.
// Emits an error if uncovered (and not suppressed by a `reason`), otherwise a
// warning if the handling is partial/non-exhaustive.
void sem_check_residue_exhaustive(SemanticCtx *ctx, ASTNode *where,
                                          SemSymbol *err_sym, ResidueCase *cases,
                                          int default_case) {
    int total = err_sym->num_err;
    int covered = 0;
    for (int i = 0; i < total; i++) {
        const char *ename = err_sym->err_names[i];
        int found = default_case;
        for (ResidueCase *rc = cases; rc; rc = rc->next) {
            if (rc->is_default) { found = 1; continue; }
            for (int j = 0; j < rc->num_err; j++) {
                if (strcmp(rc->err_names[j], ename) == 0) { found = 1; break; }
            }
            if (found) break;
        }
        if (found) covered++;
    }

    if (covered < total) {
        if (where && where->reason) {
            sem_hint(ctx, where, "purge may not be thorough: not all residue handled (handled %d of %d errors)",
                     covered, total);
        } else {
            sem_error(ctx, where, "not all residue is purged (handled %d of %d errors from '%s')",
                      covered, total, err_sym->name);
        }
    }
}

void sem_insert_implicit_cast(SemanticCtx *ctx, ASTNode **node_ptr, VarType target_type) {
    if (!node_ptr || !*node_ptr) return;
    VarType current = sem_get_node_type(ctx, *node_ptr);
    
    if (current.base == target_type.base && current.ptr_depth == target_type.ptr_depth) return;
    if (current.base == TYPE_UNKNOWN || target_type.base == TYPE_UNKNOWN) return;
    if (target_type.base == TYPE_VOID) return;

    CastNode *cast = arena_alloc_type(ctx->compiler_ctx->arena, CastNode);
    cast->base.type = NODE_CAST;
    cast->base.line = (*node_ptr)->line;
    cast->base.col = (*node_ptr)->col;
    
    cast->base.next = (*node_ptr)->next; 
    (*node_ptr)->next = NULL; 
    
    cast->var_type = target_type;
    cast->operand = *node_ptr;
    
    sem_set_node_type(ctx, (ASTNode*)cast, target_type);
    
    *node_ptr = (ASTNode*)cast;
}

// TODO split this into 
// multiple functions
void sem_scan_top_level(SemanticCtx *ctx, ASTNode *node) {
    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return;

    while (node) {
        if (node->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)node;
            // Register the error set attached via `errnum [...]` and mark the
            // function's return type as tainted.
            if (fd->has_errnum && ctx->compiler_ctx) {
                fd->ret_type.is_tainted = 1;
                for (int i = 0; i < fd->num_err; i++) {
                    const char *name = fd->err_names[i];
                    if (!hashmap_get(&ctx->compiler_ctx->error_table, name)) {
                        int id = ctx->compiler_ctx->next_error_id++;
                        hashmap_put(&ctx->compiler_ctx->error_table, strdup(name), (void*)(intptr_t)(id + 1));
                    }
                }
            }
            sem_symbolic_func_def(ctx, node);
        }
        else if (node->type == NODE_VAR_DECL) {
            sem_symbolic_var_decl(ctx, node);
        }
        else if (node->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)node;
            VarType type_class = {TYPE_CLASS, 0, arena_strdup(ctx->compiler_ctx->arena, cn->name), 0, 0, NULL, NULL, 0, 0, 0, cn->is_tainted, 0};
            SemSymbol *sym = sem_symbol_add(ctx, cn->name, SYM_CLASS, type_class);
            sym->is_is_a = cn->is_is_a;
            sym->is_has_a = cn->is_has_a;
            sym->is_pure = cn->is_pure && !cn->is_extern;
            sym->must_pure = cn->has_explicit_pure;
            sym->is_union = cn->is_union;
            if (cn->parent_name) {
                sym->parent_name = arena_strdup(ctx->compiler_ctx->arena, cn->parent_name);
            }
            if (cn->traits.count > 0) {
                sym->trait_count = cn->traits.count;
                sym->traits = arena_alloc(ctx->compiler_ctx->arena, sizeof(char*) * sym->trait_count);
                for (int i = 0; i < sym->trait_count; i++) {
                    sym->traits[i] = arena_strdup(ctx->compiler_ctx->arena, cn->traits.names[i]);
                }
            }
            sem_scan_class_members(ctx, cn, sym);
        }

        else if (node->type == NODE_STRUCT) {

        }
        else if (node->type == NODE_ENUM) {
            sem_symbolic_node_enum(ctx, node);
        }
        else if (node->type == NODE_ERRNUM) {
            extern void sem_symbolic_node_errnum(SemanticCtx *ctx, ASTNode *node);
            sem_symbolic_node_errnum(ctx, node);
        }
        else if (node->type == NODE_NAMESPACE) {
            sem_symbolic_namespace(ctx, node);
        }
        else if (node->type == NODE_COMPOUND) {
            CompoundNode *cn = (CompoundNode*)node;
            ASTNode *curr_body = cn->body;
            while (curr_body) {
                char *template_name = NULL;
                if (curr_body->type == NODE_FUNC_DEF) {
                    template_name = ((FuncDefNode*)curr_body)->name;
                } else if (curr_body->type == NODE_CLASS) {
                    template_name = ((ClassNode*)curr_body)->name;
                }
                if (template_name) {
                    VarType type_tmpl = {TYPE_UNKNOWN};
                    SemSymbol *sym = sem_symbol_add(ctx, template_name, SYM_TEMPLATE, type_tmpl);
                    sym->template_node = cn; // Should we create a new CompoundNode for each? Actually they all share the same cn, but wait...
                    // In instantiation, if it instantiates cn->body, it will instantiate EVERYTHING inside!
                    // This is actually what C++ does (for class templates with out-of-line methods, etc). Wait, if we instantiate the whole block, it will duplicate everything.
                }
                curr_body = curr_body->next;
            }
        }
        node = node->next;
    }
}
static int sem_count_class_fields(SemanticCtx *ctx, SemSymbol *sym) {
    if (!sym || sym->kind != SYM_CLASS) return 0;
    int count = 0;
    if (sym->parent_name) {
        SemSymbol *p = sem_symbol_lookup(ctx, sym->parent_name, NULL);
        if (p) count += sem_count_class_fields(ctx, p);
    }
    for (int i=0; i<sym->trait_count; i++) {
        SemSymbol *t = sem_symbol_lookup(ctx, sym->traits[i], NULL);
        if (t) count += sem_count_class_fields(ctx, t);
    }
    if (sym->inner_scope) {
        SemSymbol *s = sym->inner_scope->symbols;
        while(s) {
            if (s->kind == SYM_VAR) count++;
            s = s->next;
        }
    }
    return count;
}

static void sem_check_call_args(SemanticCtx *ctx, CallNode *node, SemSymbol *sym) {
    int arg_count = 0;
    ASTNode **curr_arg = &node->args;
    Parameter *curr_para = sym->params;
    while(*curr_arg) {
        sem_check_expr(ctx, *curr_arg);
        if (curr_para) {
            int arg_is_tainted = sem_get_node_tainted(ctx, *curr_arg);
            int param_is_pristine = curr_para->is_pristine; 

            if (arg_is_tainted && param_is_pristine) {
                sem_error(ctx, *curr_arg, "Cannot pass tainted expression to pristine parameter '%s'", curr_para->name);
            }

            if (sem_types_are_compatible(ctx,curr_para->type, sem_get_node_type(ctx, *curr_arg))) {
                sem_insert_implicit_cast(ctx, curr_arg, curr_para->type);
            } else {
                sem_error(ctx, *curr_arg, "Type '%s' is not compatible with '%s'", sem_type_to_str(sem_get_node_type(ctx, *curr_arg)), sem_type_to_str(curr_para->type));
            }
            curr_para = curr_para->next;
        } else if (sym->is_variadic) { 
            if (sem_get_node_tainted(ctx, *curr_arg)) {
                sem_error(ctx, *curr_arg, "Cannot pass tainted expression to varargs (...) of function '%s'", sym->name);
            }
        }

        curr_arg = &(*curr_arg)->next;
        arg_count++;
    }

    if (sym->param_count != arg_count && !sym->is_variadic) {
        sem_error(ctx, (ASTNode*)node, "Expected %d argument(s) for '%s', got %d", sym->param_count, sym->name, arg_count);
    }
}

void sem_check_call(SemanticCtx *ctx, CallNode *node) {
    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return;

    SemSymbol *sym = NULL;
    if (node->target) {
        sem_check_expr(ctx, node->target);
        if (node->target->type == NODE_TEMPLATE_INSTANTIATION) {
            // target->target was updated to VarRef inside sem_check_expr
            TemplateInstNode *ti = (TemplateInstNode*)node->target;
            if (ti->target->type == NODE_VAR_REF) {
                node->name = ((VarRefNode*)ti->target)->name;
            }
        } else if (node->target->type == NODE_VAR_REF) {
            node->name = ((VarRefNode*)node->target)->name;
        } else if (node->target->type == NODE_MEMBER_ACCESS) {
            node->name = ((MemberAccessNode*)node->target)->member_name;
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
    }
    if (!sym) {
        sem_error(ctx, (ASTNode*)node, "Undefined function or class '%s'", node->name);
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
        }
    }

    if (sym->kind == SYM_TEMPLATE) {
        CompoundNode *cn = sym->template_node;
        char expected_types[256] = "";
        for (int i=0; i<cn->num_type_params; i++) {
            strcat(expected_types, cn->type_params[i]);
            if (i < cn->num_type_params - 1) strcat(expected_types, ", ");
        }
        sem_error(ctx, (ASTNode*)node, "'%s' needs types [%s]", sym->name, expected_types);
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    if (sym->kind == SYM_CLASS) {
        VarType instance = {TYPE_CLASS, 1, arena_strdup(ctx->compiler_ctx->arena, sym->name), 0, 0, NULL, NULL, 0, 0, 0, 0}; 
        sem_set_node_type(ctx, (ASTNode*)node, instance);

        // Find constructor
        int ctor_found = 0;
        if (sym->inner_scope) {
            SemSymbol *constructor = sym->inner_scope->symbols;
            while (constructor) {
                if (strcmp(constructor->name, sym->name) == 0 || strcmp(constructor->name, "init") == 0) {
                     sem_check_call_args(ctx, node, constructor);
                     ctor_found = 1;
                     break;
                }
                constructor = constructor->next;
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
                    SemSymbol *constructor = trait_sym->inner_scope->symbols;
                    while (constructor) {
                        if (strcmp(constructor->name, trait_sym->name) == 0 || strcmp(constructor->name, "init") == 0) {
                            sem_check_call_args(ctx, node, constructor);
                            ctor_found = 1;
                            sem_warning(ctx, (ASTNode*)node, "no initialization from %s, using %s as a constructor", sym->name, trait_sym->name);
                            break;
                        }
                        constructor = constructor->next;
                    }
                    if (ctor_found) break;
                }
            }
        }

        if (!ctor_found) {
             if (sym->trait_count > 0) {
                 sem_warning(ctx, (ASTNode*)node, "no initialization from %s, using %s as a constructor", sym->name, sym->traits[0]);
             }
             // Basic validation for implicit constructor: should match number of fields
             int total_fields = sem_count_class_fields(ctx, sym);
             int arg_count = 0;
             ASTNode *a = node->args;
             while(a) {
                 sem_check_expr(ctx, a);
                 a = a->next;
                 arg_count++;
             }
             if (arg_count != total_fields) {
                 int is_copy = 0;
                 if (arg_count == 1) {
                     VarType arg_type = sem_get_node_type(ctx, node->args);
                     // printf("DEBUG: copy check: base=%d expected=%d class_name=%s sym_name=%s\n", arg_type.base, TYPE_CLASS, arg_type.class_name ? arg_type.class_name : "(null)", sym->name);
                     if (arg_type.base == TYPE_CLASS && arg_type.class_name && strcmp(arg_type.class_name, sym->name) == 0) {
                         is_copy = 1;
                     }
                 }
                 if (!is_copy) {
                     sem_error(ctx, (ASTNode*)node, "Expected %d argument(s) for implicit constructor of '%s', got %d", total_fields, sym->name, arg_count);
                 }
             }
        }
    }
 else if (sym->kind == SYM_FUNC && sym->is_flux) {

        // Rewrite flux generator return type dynamically for iterators to intercept!
        char buf[256];
        snprintf(buf, sizeof(buf), "FluxCtx_%s", sym->name);
        VarType flux_type = {TYPE_CLASS, 1, arena_strdup(ctx->compiler_ctx->arena, buf), 0, 0, NULL, NULL, 0, 0, 0, 0};
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

void sem_check_binary_op(SemanticCtx *ctx, BinaryOpNode *node) {
    sem_check_expr(ctx, node->left);
    sem_check_expr(ctx, node->right);
    
    VarType l = sem_get_node_type(ctx, node->left);
    VarType r = sem_get_node_type(ctx, node->right);
    
    // Operator Overloading Check
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", TOKEN_INFOP, node->op);
    SemSymbol *sym = sem_symbol_lookup(ctx, name_buf, NULL);
    if (sym && sym->kind == SYM_FUNC) {
        ASTNode *args = node->left;
        args->next = node->right;
        node->right->next = NULL;
        SemSymbol *resolved = sem_resolve_overload(ctx, &args, NULL, sym, NULL);
        if (resolved) {
            node->overloaded_func_name = arena_strdup(ctx->compiler_ctx->arena, resolved->mangled_name ? resolved->mangled_name : resolved->name);
            sem_set_node_type(ctx, (ASTNode*)node, resolved->type);
            node->left = args;
            node->right = args->next;
            node->left->next = NULL;
            if (node->right) node->right->next = NULL;
            return;
        }
        // Restore next pointers if not resolved
        node->left->next = NULL;
        node->right->next = NULL;
    }

    if (node->op == TOKEN_QUESTION || node->op == TOKEN_QUESTION_QUESTION) {
        if (node->fallback_err_name) {
            // It filters a specific error. Without full union types, we just keep it tainted if the left was tainted.
            sem_set_node_tainted(ctx, (ASTNode*)node, sem_get_node_tainted(ctx, node->left));
        } else {
            // Catch-all
            sem_set_node_tainted(ctx, (ASTNode*)node, 0); // Result is pristine!
        }
    } else if (sem_get_node_tainted(ctx, node->left) || sem_get_node_tainted(ctx, node->right)) {
        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
    } else if (node->op == TOKEN_SLASH) {
        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
    }

    if (l.base == TYPE_UNKNOWN || r.base == TYPE_UNKNOWN) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    if ((l.base == TYPE_VOID && l.ptr_depth == 0) || (r.base == TYPE_VOID && r.ptr_depth == 0)) {
        sem_error(ctx, (ASTNode*)node, "Operand of binary expression cannot be 'void'");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }
    
    if (node->op == TOKEN_AND_AND || node->op == TOKEN_OR_OR) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }
    
    if (node->op == TOKEN_QUESTION || node->op == TOKEN_QUESTION_QUESTION) {
        // Fallback uses the type of the left hand side, but preserve the taint state we just calculated
        int calculated_taint = sem_get_node_tainted(ctx, (ASTNode*)node);
        VarType res_ty = l;
        res_ty.is_tainted = calculated_taint;
        sem_set_node_type(ctx, (ASTNode*)node, res_ty);
        
        // Register the error ID if it's a specific fallback
        if (node->fallback_err_name) {
            void *err_val = hashmap_get(&ctx->compiler_ctx->error_table, node->fallback_err_name);
            if (!err_val && strncmp(node->fallback_err_name, "Err", 3) == 0) {
                int id = ctx->compiler_ctx->next_error_id++;
                hashmap_put(&ctx->compiler_ctx->error_table, strdup(node->fallback_err_name), (void*)(intptr_t)(id + 1));
            }
        }

        return;
    }
    
    // todo fix this casting!
    if (node->op == TOKEN_EQ || node->op == TOKEN_NEQ || 
        node->op == TOKEN_LT || node->op == TOKEN_GT || 
        node->op == TOKEN_LTE || node->op == TOKEN_GTE) {
        
        if (is_numeric(l) && is_numeric(r)) {
            VarType target_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            if (l.base == TYPE_LONG_DOUBLE || r.base == TYPE_LONG_DOUBLE) target_type.base = TYPE_LONG_DOUBLE;
            else if (l.base == TYPE_DOUBLE || r.base == TYPE_DOUBLE) target_type.base = TYPE_DOUBLE;
            else if (l.base == TYPE_SINGLE || r.base == TYPE_SINGLE) target_type.base = TYPE_SINGLE;
            else if (l.base == TYPE_LONG || r.base == TYPE_LONG) target_type.base = TYPE_LONG;
            else if (l.base == TYPE_UNSIGNED_INT || r.base == TYPE_UNSIGNED_INT) target_type.base = TYPE_UNSIGNED_INT;
            if (l.is_tainted || r.is_tainted) target_type.is_tainted = 1;
            sem_insert_implicit_cast(ctx, &node->left, target_type);
            sem_insert_implicit_cast(ctx, &node->right, target_type);
        }
        
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }
    
    if (is_numeric(l) && is_numeric(r)) {
        VarType target_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
        
        if (l.base == TYPE_LONG_DOUBLE || r.base == TYPE_LONG_DOUBLE) target_type.base = TYPE_LONG_DOUBLE;
        else if (l.base == TYPE_DOUBLE || r.base == TYPE_DOUBLE) target_type.base = TYPE_DOUBLE;
        else if (l.base == TYPE_SINGLE || r.base == TYPE_SINGLE) target_type.base = TYPE_SINGLE;
        else if (l.base == TYPE_LONG || r.base == TYPE_LONG) target_type.base = TYPE_LONG;
        else if (l.base == TYPE_UNSIGNED_INT || r.base == TYPE_UNSIGNED_INT) target_type.base = TYPE_UNSIGNED_INT;
        if (l.is_tainted || r.is_tainted) target_type.is_tainted = 1;
        
        sem_insert_implicit_cast(ctx, &node->left, target_type);
        sem_insert_implicit_cast(ctx, &node->right, target_type);
        
        sem_set_node_type(ctx, (ASTNode*)node, target_type);
    } 
    else if (is_pointer(l) && is_integer(r)) {
         sem_set_node_type(ctx, (ASTNode*)node, l);
    }
    else if (is_integer(l) && is_pointer(r)) {
         sem_set_node_type(ctx, (ASTNode*)node, r);
    }
    else if ((l.base == TYPE_CLASS && l.class_name && strcmp(l.class_name, "string") == 0) || (r.base == TYPE_CLASS && r.class_name && strcmp(r.class_name, "string") == 0)) {
         if (node->op == TOKEN_PLUS) 
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){ .base = TYPE_CLASS, .class_name = (char*)"string" });
         else {
            sem_error(ctx, (ASTNode*)node, "Invalid operation on strings");
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
         }
    }
    else {
        sem_error(ctx, (ASTNode*)node, "Invalid operands for binary operator");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    }
}

void sem_check_expr(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return;
    
    switch(node->type) {
        case NODE_LITERAL: {
            LiteralNode *lit = (LiteralNode*)node;
            sem_set_node_type(ctx, node, lit->var_type);
            break;
        }
        case NODE_SIZEOF:
        case NODE_ALIGNOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                sem_check_expr(ctx, sn->operand);
            } else if (sn->target_type.base == TYPE_CLASS && sn->target_type.ptr_depth == 0) {
                SemSymbol *sym = sem_symbol_lookup(ctx, sn->target_type.class_name, NULL);
                if (!sym || sym->kind != SYM_CLASS) {
                    sem_error(ctx, node, "Unknown class type '%s' in sizeof", sn->target_type.class_name);
                }
            }
            VarType size_type = {TYPE_UNSIGNED_LONG, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            sem_set_node_type(ctx, node, size_type);
            break;
        }
        case NODE_VAR_REF: {
            sem_check_var_ref(ctx, node);
            break;
        }
        case NODE_BINARY_OP: sem_check_binary_op(ctx, (BinaryOpNode*)node); break;
        case NODE_UNARY_OP:
            sem_check_unary_op_switch(ctx, node);
            break;
        case NODE_CALL: sem_check_call(ctx, (CallNode*)node); break;
        case NODE_NAMED_ARG: {
            NamedArgNode *narg = (NamedArgNode*)node;
            sem_check_expr(ctx, narg->value);
            sem_set_node_type(ctx, node, sem_get_node_type(ctx, narg->value));
            if (sem_get_node_tainted(ctx, narg->value)) sem_set_node_tainted(ctx, node, 1);
            break;
        }
        case NODE_MEMBER_ACCESS: sem_check_member_access(ctx, (MemberAccessNode*)node); break;
        case NODE_ARRAY_ACCESS: {
            sem_check_array_access(ctx, node);
            break;
        }
        case NODE_CAST: {
            CastNode *cn = (CastNode*)node;
            cn->custom_cast_method = NULL;
            sem_check_expr(ctx, cn->operand);
            
            if (sem_get_node_tainted(ctx, cn->operand)) {
                sem_set_node_tainted(ctx, node, 1);
            }
            
            VarType op_t = sem_get_node_type(ctx, cn->operand);
            if (op_t.base == TYPE_VOID && op_t.ptr_depth == 0) {
                sem_error(ctx, node, "Cannot cast 'void' value");
            }
            
            if (op_t.base == TYPE_CLASS && op_t.class_name) {
                char as_name[256];
                if (cn->var_type.base == TYPE_CLASS || cn->var_type.base == TYPE_UNKNOWN) {
                    snprintf(as_name, sizeof(as_name), "as_%s", cn->var_type.class_name ? cn->var_type.class_name : "");
                } else if (cn->var_type.base == TYPE_INT) {
                    snprintf(as_name, sizeof(as_name), "as_int");
                } else if (cn->var_type.base == TYPE_SINGLE) {
                    snprintf(as_name, sizeof(as_name), "as_float");
                } else {
                    snprintf(as_name, sizeof(as_name), "as_type%d", cn->var_type.base);
                }
                
                SemSymbol *class_sym = sem_symbol_lookup(ctx, op_t.class_name, NULL);
                if (class_sym && class_sym->inner_scope) {
                    SemSymbol *member = class_sym->inner_scope->symbols;
                    while (member) {
                        if (member->kind == SYM_FUNC && strcmp(member->name, as_name) == 0) {
                            // Found custom cast operator!
                            char mangled[512];
                            snprintf(mangled, sizeof(mangled), "%s_%s", op_t.class_name, as_name);
                            cn->custom_cast_method = arena_strdup(ctx->compiler_ctx->arena, mangled);
                            break;
                        }
                        member = member->next;
                    }
                }
            }
            
            if (!cn->custom_cast_method) {
                if (op_t.base == TYPE_CLASS && op_t.class_name) {
                    SemSymbol *class_sym = sem_symbol_lookup(ctx, op_t.class_name, NULL);
                    if (class_sym && class_sym->is_union && class_sym->inner_scope) {
                        SemSymbol *f = class_sym->inner_scope->symbols;
                        while (f) {
                            if (f->kind == SYM_VAR && sem_types_are_compatible(ctx, f->type, cn->var_type)) {
                                MemberAccessNode ma;
                                memset(&ma, 0, sizeof(MemberAccessNode));
                                ma.base.type = NODE_MEMBER_ACCESS;
                                ma.base.line = node->line;
                                ma.base.col = node->col;
                                ma.object = cn->operand;
                                ma.member_name = arena_strdup(ctx->compiler_ctx->arena, f->name);
                                
                                sem_set_node_type(ctx, node, f->type);
                                memcpy(node, &ma, sizeof(MemberAccessNode));
                                return; // Successfully replaced!
                            }
                            f = f->next;
                        }
                    }
                }
                if (!sem_types_are_compatible(ctx, cn->var_type, op_t)) {
                    char *t1 = sem_type_to_str(op_t);
                    char *t2 = sem_type_to_str(cn->var_type);
                    sem_error(ctx, node, "Cannot cast '%s' to '%s' (types are not compatible)", t1, t2);
                }
            }

            sem_set_node_type(ctx, node, cn->var_type);
            break;
        }
        case NODE_METHOD_CALL: {
            sem_check_method_call(ctx, (MethodCallNode*)node);
            break;
        }
        // Fix this shit
        case NODE_ARRAY_LIT: {
            ArrayLitNode *al = (ArrayLitNode*)node;
            ASTNode *el = al->elements;
            VarType elem_type = {TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            int count = 1; 

            if (el) {
                sem_check_expr(ctx, el);
                elem_type = sem_get_node_type(ctx, el);
                el = el->next;
            }
            while(el) {
              sem_check_expr(ctx, el);
              el = el->next;
                count++; 
            }
            elem_type.ptr_depth = 0;
            elem_type.array_size = count;
            sem_set_node_type(ctx, node, elem_type);
            break;        }
        case NODE_INC_DEC: {
            IncDecNode *id = (IncDecNode*)node;
            sem_check_expr(ctx, id->target);
            VarType t = sem_get_node_type(ctx, id->target);
            
            // Operator Overloading Check
            char name_buf[64];
            snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", id->is_prefix ? TOKEN_PREMUT : TOKEN_SUFMUT, id->op);
            SemSymbol *sym = NULL;
            int is_method = 0;
            if (t.base == TYPE_CLASS && t.ptr_depth == 0 && t.class_name) {
                SemSymbol *class_sym = sem_symbol_lookup(ctx, t.class_name, NULL);
                if (class_sym && class_sym->inner_scope) {
                    SemSymbol *s = class_sym->inner_scope->symbols;
                    while (s) {
                        if (strcmp(s->name, name_buf) == 0) { sym = s; is_method = 1; break; }
                        s = s->next;
                    }
                    if (!sym) {
                        snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", id->is_prefix ? TOKEN_PREFOP : TOKEN_SUFFOP, id->op);
                        s = class_sym->inner_scope->symbols;
                        while (s) {
                            if (strcmp(s->name, name_buf) == 0) { sym = s; is_method = 1; break; }
                            s = s->next;
                        }
                    }
                }
            }
            if (!sym) {
                snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", id->is_prefix ? TOKEN_PREMUT : TOKEN_SUFMUT, id->op);
                sym = sem_symbol_lookup(ctx, name_buf, NULL);
                if (!sym) {
                    snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", id->is_prefix ? TOKEN_PREFOP : TOKEN_SUFFOP, id->op);
                    sym = sem_symbol_lookup(ctx, name_buf, NULL);
                }
            }
            if (sym && sym->kind == SYM_FUNC) {
                if (is_method) {
                    ASTNode *no_args = NULL;
                    SemSymbol *resolved = sem_resolve_overload(ctx, &no_args, NULL, sym, NULL);
                    if (resolved) {
                        id->overloaded_func_name = arena_strdup(ctx->compiler_ctx->arena, resolved->mangled_name ? resolved->mangled_name : resolved->name);
                        sem_set_node_type(ctx, (ASTNode*)node, resolved->type);
                        break;
                    }
                } else {
                    UnaryOpNode *addr_of = arena_alloc_type(ctx->compiler_ctx->arena, UnaryOpNode);
                    memset(addr_of, 0, sizeof(UnaryOpNode));
                    addr_of->base.type = NODE_UNARY_OP;
                    addr_of->op = TOKEN_AND;
                    addr_of->operand = id->target;
                    addr_of->base.line = node->line;
                    addr_of->base.col = node->col;
                    VarType ptr_type = t;
                    ptr_type.ptr_depth++;
                    sem_set_node_type(ctx, (ASTNode*)addr_of, ptr_type);
                    
                    ASTNode *args = (ASTNode*)addr_of;
                    args->next = NULL;
                    SemSymbol *resolved = sem_resolve_overload(ctx, &args, NULL, sym, NULL);
                    if (resolved) {
                        id->overloaded_func_name = arena_strdup(ctx->compiler_ctx->arena, resolved->mangled_name ? resolved->mangled_name : resolved->name);
                        sem_set_node_type(ctx, (ASTNode*)node, resolved->type);
                        id->target = args;
                        id->target->next = NULL;
                        break;
                    }
                }
            }

            if (!is_numeric(t) && !is_pointer(t) && t.base != TYPE_UNKNOWN) {
                sem_error(ctx, node, "Cannot increment/decrement non-numeric/non-pointer type");
            }
            if (sem_get_node_tainted(ctx, id->target)) sem_set_node_tainted(ctx, node, 1);
            sem_set_node_type(ctx, node, t);
            break;
        }
        case NODE_ASSIGN: {
            AssignNode *an = (AssignNode*)node;
            sem_check_assign(ctx, an); 
            VarType t;
            if (an->name) {
                SemSymbol *sym = sem_symbol_lookup(ctx, an->name, NULL);
                t = sym ? sym->type : (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            } else {
                t = sem_get_node_type(ctx, an->target);
            }
            sem_set_node_type(ctx, node, t);
            break;
        }

        case NODE_TYPEOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                sem_check_expr(ctx, sn->operand);
            }
            sem_set_node_type(ctx, node, (VarType){ .base = TYPE_INT, .ptr_depth = 0 });
            break;
        }
        case NODE_HAS_METHOD:
        case NODE_HAS_ATTRIBUTE: {
            UnaryOpNode *un = (UnaryOpNode*)node;
            
            // Do not check the operand because it might be a trait or class name which fails normally.
            // But wait, if it's a class name, it resolves to a type.
            // For now we will manually look it up.
            char *target_name = NULL;
            if (un->operand->type == NODE_VAR_REF) {
                target_name = ((VarRefNode*)un->operand)->name;
            } else if (un->operand->type == NODE_MEMBER_ACCESS) {
                MemberAccessNode *ma = (MemberAccessNode*)un->operand;
                sem_check_expr(ctx, ma->object);
                VarType obj_t = sem_get_node_type(ctx, ma->object);
                if (obj_t.base == TYPE_CLASS && obj_t.class_name) target_name = obj_t.class_name;
            }
            
            ASTNode *head = NULL;
            ASTNode **curr = &head;
            int count = 0;
            
            if (target_name) {
                SemSymbol *class_sym = sem_symbol_lookup(ctx, target_name, NULL);
                if (class_sym && class_sym->inner_scope) {
                    SemSymbol *s = class_sym->inner_scope->symbols;
                    while (s) {
                        bool match = false;
                        if (node->type == NODE_HAS_METHOD && s->kind == SYM_FUNC) match = true;
                        if (node->type == NODE_HAS_ATTRIBUTE && s->kind == SYM_VAR) match = true;
                        
                        if (match) {
                            LiteralNode *ln = arena_alloc(ctx->compiler_ctx->arena, sizeof(LiteralNode));
                            ln->base.type = NODE_LITERAL;
                            if (ctx->compiler_ctx->settings.double_quote_as_string) {
                                ln->var_type.base = TYPE_CLASS;
                                ln->var_type.class_name = arena_strdup(ctx->compiler_ctx->arena, "string");
                                ln->var_type.ptr_depth = 0;
                            } else {
                                ln->var_type.base = TYPE_CHAR;
                                ln->var_type.class_name = NULL;
                                ln->var_type.ptr_depth = 1;
                            }
                            ln->var_type.array_size = 0;
                            ln->val.str_val = arena_strdup(ctx->compiler_ctx->arena, s->name);
                            sem_set_node_type(ctx, (ASTNode*)ln, ln->var_type);
                            *curr = (ASTNode*)ln;
                            curr = &(*curr)->next;
                            count++;
                        }
                        s = s->next;
                    }
                }
            }
            
            // Insert the length at index 0
            ASTNode *len_ast = NULL;
            if (ctx->compiler_ctx->settings.double_quote_as_string) {
                char length_str[16];
                snprintf(length_str, sizeof(length_str), "%d", count);
                LiteralNode *len_node = arena_alloc(ctx->compiler_ctx->arena, sizeof(LiteralNode));
                len_node->base.type = NODE_LITERAL;
                len_node->var_type.base = TYPE_CLASS;
                len_node->var_type.class_name = arena_strdup(ctx->compiler_ctx->arena, "string");
                len_node->var_type.ptr_depth = 0;
                len_node->var_type.array_size = 0;
                len_node->val.str_val = arena_strdup(ctx->compiler_ctx->arena, length_str);
                sem_set_node_type(ctx, (ASTNode*)len_node, len_node->var_type);
                len_ast = (ASTNode*)len_node;
            } else {
                LiteralNode *int_node = arena_alloc(ctx->compiler_ctx->arena, sizeof(LiteralNode));
                int_node->base.type = NODE_LITERAL;
                int_node->var_type.base = TYPE_INT;
                int_node->var_type.ptr_depth = 0;
                int_node->var_type.array_size = 0;
                int_node->val.int_val = count;
                
                CastNode *cast_node = arena_alloc(ctx->compiler_ctx->arena, sizeof(CastNode));
                cast_node->base.type = NODE_CAST;
                cast_node->var_type.base = TYPE_CHAR;
                cast_node->var_type.ptr_depth = 1;
                cast_node->var_type.array_size = 0;
                sem_set_node_type(ctx, (ASTNode*)int_node, int_node->var_type);
                cast_node->operand = (ASTNode*)int_node;
                sem_set_node_type(ctx, (ASTNode*)cast_node, cast_node->var_type);
                len_ast = (ASTNode*)cast_node;
            }
            
            // Terminate the symbol list
            *curr = NULL;
            
            len_ast->next = head;
            head = len_ast;
            count++;
            
            node->type = NODE_ARRAY_LIT;
            ArrayLitNode *an = (ArrayLitNode*)node;
            an->elements = head;
            
            VarType arr_t;
            if (ctx->compiler_ctx->settings.double_quote_as_string) {
                arr_t = (VarType){ .base = TYPE_CLASS, .class_name = arena_strdup(ctx->compiler_ctx->arena, "string"), .ptr_depth = 0, .array_size = count };
            } else {
                arr_t = (VarType){ .base = TYPE_CHAR, .class_name = NULL, .ptr_depth = 1, .array_size = count };
            }
            sem_set_node_type(ctx, node, arr_t);
            break;
        }
        case NODE_TEMPLATE_INSTANTIATION: {
            TemplateInstNode *ti = (TemplateInstNode*)node;
            char target_name[256] = "";
            if (ti->target->type == NODE_VAR_REF) {
                snprintf(target_name, sizeof(target_name), "%s", ((VarRefNode*)ti->target)->name);
            } else if (ti->target->type == NODE_MEMBER_ACCESS) {
                MemberAccessNode *ma = (MemberAccessNode*)ti->target;
                if (ma->object->type == NODE_VAR_REF) {
                    snprintf(target_name, sizeof(target_name), "%s.%s", ((VarRefNode*)ma->object)->name, ma->member_name);
                } else {
                    sem_error(ctx, node, "Unsupported member access in template instantiation");
                    break;
                }
            } else {
                sem_error(ctx, node, "Expected identifier for template instantiation");
                break;
            }
            SemScope *found_in_scope = NULL;
            SemSymbol *sym = sem_symbol_lookup(ctx, target_name, &found_in_scope);
            if (!sym || sym->kind != SYM_TEMPLATE) {
                // Fallback to array access / component access!
                if (ti->num_template_types == 1) {
                    LiteralNode *index_ln = arena_alloc(ctx->compiler_ctx->arena, sizeof(LiteralNode));
                    index_ln->base.type = NODE_LITERAL;
                    index_ln->var_type = ti->template_types[0];
                    index_ln->val.long_val = ti->template_types[0].base;
                    index_ln->base.line = node->line;
                    index_ln->base.col = node->col;
                    
                    ArrayAccessNode *aa = (ArrayAccessNode*)node;
                    aa->base.type = NODE_ARRAY_ACCESS;
                    // aa->target is already ti->target, so we don't need to change it
                    aa->target = ti->target;
                    aa->index = (ASTNode*)index_ln;
                    
                    sem_check_expr(ctx, node); // Check again as array access
                    break;
                }
                
                sem_error(ctx, node, "'%s' is not a template", target_name);
                break;
            }
            CompoundNode *cn = sym->template_node;
            if (ti->num_template_types != cn->num_type_params) {
                sem_error(ctx, node, "Template '%s' expects %d types, got %d", target_name, cn->num_type_params, ti->num_template_types);
                break;
            }
            
            for (int i = 0; i < ti->num_template_types; i++) {
                if (cn->num_allowed && cn->num_allowed[i] > 0) {
                    int match = 0;
                    for (int j = 0; j < cn->num_allowed[i]; j++) {
                        int is_compat = sem_types_are_equal(ti->template_types[i], cn->allowed_types[i][j]);
                        if (is_compat) {
                            match = 1;
                            break;
                        }
                    }
                    if (!match) {
                        char *t1 = sem_type_to_str(ti->template_types[i]);
                        sem_error(ctx, node, "Type '%s' is not allowed for template parameter '%s'", t1, cn->type_params[i]);
                    }
                }
            }

            // Generate mangled name
            char mangled[1024];
            snprintf(mangled, sizeof(mangled), "%s", target_name);
            for (int i = 0; i < ti->num_template_types; i++) {
                char *t_str = sem_type_to_str(ti->template_types[i]);
                strncat(mangled, "_", sizeof(mangled) - strlen(mangled) - 1);
                strncat(mangled, t_str, sizeof(mangled) - strlen(mangled) - 1);
            }
            
            SemSymbol *inst_sym = sem_symbol_lookup(ctx, mangled, NULL);
            if (!inst_sym) {
            // 1. Collect all top-level names in the block and their mangled names
            int num_renames = 0;
            char *rename_from[32];
            char *rename_to[32];
            ASTNode *cn_curr = cn->body;
            while (cn_curr) {
                char *base_name = NULL;
                if (cn_curr->type == NODE_FUNC_DEF) base_name = ((FuncDefNode*)cn_curr)->name;
                else if (cn_curr->type == NODE_CLASS) base_name = ((ClassNode*)cn_curr)->name;
                
                if (base_name) {
                    if (num_renames >= 32) {
                        sem_error(ctx, node, "Template has too many top-level symbols to instantiate");
                        break;
                    }
                    rename_from[num_renames] = base_name;
                    char node_mangled[1024];
                    snprintf(node_mangled, sizeof(node_mangled), "%s", base_name);
                    for (int i = 0; i < ti->num_template_types; i++) {
                        char *t_str = sem_type_to_str(ti->template_types[i]);
                        size_t nm_len = strlen(node_mangled);
                        if (nm_len + 1 < sizeof(node_mangled)) {
                            snprintf(node_mangled + nm_len, sizeof(node_mangled) - nm_len, "_%s", t_str);
                        }
                    }
                    rename_to[num_renames] = arena_strdup(ctx->compiler_ctx->arena, node_mangled);
                    num_renames++;
                }
                cn_curr = cn_curr->next;
            }
            
            for (int i = 0; i < num_renames; i++) {
                printf("Rename: %s -> %s\n", rename_from[i], rename_to[i]);
            }
            
            // 2. Clone the body with replacements AND renames
            ASTNode *cloned_body = ast_clone(ctx->compiler_ctx, cn->body, cn->type_params, ti->template_types, ti->num_template_types, rename_from, rename_to, num_renames);
                
                printf("DEBUG: &ctx->ast_tail=%p, ctx->ast_tail=%p, *ctx->ast_tail=%p\n", &ctx->ast_tail, ctx->ast_tail, ctx->ast_tail ? *ctx->ast_tail : NULL); if (ctx->ast_tail) {
                    *ctx->ast_tail = cloned_body;
                    while (*ctx->ast_tail) {
                        ctx->ast_tail = &(*ctx->ast_tail)->next;
                    }
                }
                
                // Add to global AST? Just scan and check it now!
                SemScope *old_scope = ctx->current_scope;
                ctx->current_scope = found_in_scope ? found_in_scope : ctx->global_scope;
                sem_scan_top_level(ctx, cloned_body);
                // Also check it now, but for all nodes in the cloned body!
                ASTNode *curr = cloned_body;
                while (curr) {
                    sem_check_node(ctx, curr);
                    curr = curr->next;
                }
                ctx->current_scope = old_scope;
                inst_sym = sem_symbol_lookup(ctx, mangled, NULL);
            }
            
            // Replace the current node with a VarRef to the mangled name, so codegen just calls the instantiated function/class
            // Wait, this is an expression! A template instantiation `map[int]` resolves to the function name itself.
            // So its type should be the type of `inst_sym`.
            if (inst_sym) {
                sem_set_node_type(ctx, node, inst_sym->type);
                // Also update the target so Codegen emits the mangled name
                VarRefNode *new_vr = arena_alloc(ctx->compiler_ctx->arena, sizeof(VarRefNode));
                new_vr->base.type = NODE_VAR_REF;
                new_vr->name = arena_strdup(ctx->compiler_ctx->arena, mangled);
                new_vr->base.line = node->line;
                new_vr->base.col = node->col;
                ti->target = (ASTNode*)new_vr;
            }
            break;
        }
        default: break;
    }
}

void sem_check_stmt(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_PURGE: {
            PurgeNode *pn = (PurgeNode*)node;
            
            if (ctx->current_func_sym) {
                if (ctx->current_func_sym->has_explicit_pristine) {
                    sem_error(ctx, node, "Pristine function '%s' cannot use purge", ctx->current_func_sym->name);
                } else if (!ctx->current_func_sym->type.is_tainted) {
                    sem_hint(ctx, node, "Function '%s' uses purge, consider marking it as tainted", ctx->current_func_sym->name);
                }
            }
            
            if (pn->msg->type == NODE_VAR_REF) {
                VarRefNode *var = (VarRefNode*)pn->msg;
                // It's an error identifier!
                if (!hashmap_get(&ctx->compiler_ctx->error_table, var->name)) {
                    int id = ctx->compiler_ctx->next_error_id++;
                    hashmap_put(&ctx->compiler_ctx->error_table, strdup(var->name), (void*)(intptr_t)(id + 1));
                }
                // No further type check on var because it's just an error identifier
            } else {
                sem_error(ctx, node, "purge requires an error identifier (e.g., ErrDivisionByZero)");
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
                        sem_error(ctx, node, "Return type mismatch");
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
            
            sem_scope_enter(ctx, 0, (VarType){0});
            sem_check_block(ctx, ifn->then_body);
            sem_scope_exit(ctx);
            
            if (ifn->else_body) {
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
            sem_check_expr(ctx, wn->condition);
            ctx->in_loop++;
            
            sem_scope_enter(ctx, 0, (VarType){0});
            sem_check_block(ctx, wn->body);
            sem_scope_exit(ctx);
            
            ctx->in_loop--;
            break;
        }
        case NODE_LOOP: {
            LoopNode *ln = (LoopNode*)node;
            sem_check_expr(ctx, ln->iterations);
            ctx->in_loop++;
            
            sem_scope_enter(ctx, 0, (VarType){0});
            sem_check_block(ctx, ln->body);
            sem_scope_exit(ctx);
            
            ctx->in_loop--;
            break;
        }
        case NODE_FOR_IN: {
            sem_check_for_in(ctx, node);
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
        case NODE_ARRAY_ACCESS:
        case NODE_VECTOR_ACCESS:
        case NODE_BINARY_OP:
        case NODE_UNARY_OP:
        case NODE_VAR_REF:
        case NODE_LITERAL:
        case NODE_ARRAY_LIT:
        case NODE_CAST:
        case NODE_TYPEOF:
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

void sem_check_block(SemanticCtx *ctx, ASTNode *block) {
    ASTNode *curr = block;
    while (curr) {
        sem_check_node(ctx, curr);
        curr = curr->next;
    }
}

void sem_check_node(SemanticCtx *ctx, ASTNode *node) {
    if (node->type == NODE_FUNC_DEF) sem_check_func_def(ctx, (FuncDefNode*)node);
    else if (node->type == NODE_CLASS) {
        ClassNode *cn = (ClassNode*)node;
        if (cn->is_abstract && cn->is_exact) sem_error(ctx, node, "Class cannot be both abstract and exact");
        if (cn->is_method_class && cn->is_container) sem_error(ctx, node, "Class cannot be both method and container");
        SemSymbol *sym = sem_symbol_lookup(ctx, cn->name, NULL);
        if (cn->parent_name) {
            SemSymbol *ps = sem_symbol_lookup(ctx, cn->parent_name, NULL);
            if (!ps || ps->kind != SYM_CLASS) {
                sem_error(ctx, node, "Undefined parent class '%s'", cn->parent_name);
            }
        }
        for (int i = 0; i < cn->traits.count; i++) {
            SemSymbol *ts = sem_symbol_lookup(ctx, cn->traits.names[i], NULL);
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
    else {
        sem_check_stmt(ctx, node);
    }
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
                        if (p->name && strcmp(p->name, narg->name) == 0) { found = i; break; }
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
            sem_error(ctx, err_node, "No matching overload found for function '%s'", first_sym->name);
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
            }
            p_curr = &(*p_curr)->next;
        }
    }
    
    return best_match;
}
