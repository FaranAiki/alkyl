#include "semantic.h"
#include <stdio.h>

// Defined in check.c.
extern SemSymbol* sem_get_errnum_func_sym(SemanticCtx *ctx, ASTNode *node);
extern void sem_check_residue_exhaustive(SemanticCtx *ctx, ASTNode *where,
                                         SemSymbol *err_sym, ResidueCase *cases,
                                         int default_case);

void sem_check_implicit_cast(SemanticCtx *ctx, ASTNode *node, VarType dest, VarType src) {
    int dest_is_str = (dest.base == TYPE_CLASS && dest.class_name && strcmp(dest.class_name, "string") == 0 && dest.ptr_depth == 0);
    int src_is_char = (src.base == TYPE_CHAR && (src.ptr_depth > 0 || src.array_size > 0));
    
    int dest_is_char = (dest.base == TYPE_CHAR && (dest.ptr_depth > 0 || dest.array_size > 0));
    int src_is_str = (src.base == TYPE_CLASS && src.class_name && strcmp(src.class_name, "string") == 0 && src.ptr_depth == 0);
    
    if (dest_is_str && src_is_char) {
        sem_info(ctx, node, "Implicit cast from 'char%s' to 'string'", (src.array_size > 0) ? "[]" : "*");
    } else if (dest_is_char && src_is_str) {
        sem_info(ctx, node, "Implicit cast from 'string' to 'char%s'", (dest.array_size > 0) ? "[]" : "*");
        
        if (node->type == NODE_LITERAL) {
            LiteralNode *lit = (LiteralNode*)node;
            if (lit->var_type.base == TYPE_CLASS && lit->var_type.class_name && strcmp(lit->var_type.class_name, "string") == 0 && lit->val.str_val) {
                sem_hint(ctx, node, "Use c\"%s\" for a C-style string", lit->val.str_val);
                return;
            }
        }
        sem_hint(ctx, node, "Use c\"...\" for a C-style string");
    }
}

void sem_check_var_decl(SemanticCtx *ctx, VarDeclNode *node, int register_sym) {
    if (node->is_array && node->var_type.ptr_depth == 0) {
        ASTNode *dim = node->array_size;
        while(dim) {
            node->var_type.ptr_depth++;
            dim = dim->next;
        }
        if (node->array_size && node->array_size->type == NODE_LITERAL) {
             node->var_type.array_size = (int)((LiteralNode*)node->array_size)->val.long_val;
        }
    }

    if (node->var_type.base == TYPE_CLASS && node->var_type.class_name) {
        SemSymbol *sym = sem_symbol_lookup(ctx, node->var_type.class_name, NULL);
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
            sem_error(ctx, (ASTNode*)node, "'%s' needs types [%s]", node->var_type.class_name, expected_types);
            node->var_type.base = TYPE_UNKNOWN;
        }
    }
    if (node->var_type.class_name) {
        char *bracket = strchr(node->var_type.class_name, '[');
        if (bracket) {
            char mangled[512];
            snprintf(mangled, sizeof(mangled), "%s", node->var_type.class_name);
            for (int i=0; mangled[i]; i++) {
                if (mangled[i] == '[') mangled[i] = '_';
                else if (mangled[i] == ']') mangled[i] = '\0';
                else if (mangled[i] == ',' || mangled[i] == ' ') mangled[i] = '_';
            }
            char final_mangled[512];
            int j = 0;
            for (int i=0; mangled[i] && j < 511; i++) {
                if (mangled[i] == '_' && mangled[i+1] == '_') continue;
                final_mangled[j++] = mangled[i];
            }
            final_mangled[j] = '\0';
            node->var_type.class_name = arena_strdup(ctx->compiler_ctx->arena, final_mangled);
        }
    }

    if (node->initializer) {
        sem_check_expr(ctx, node->initializer);
        VarType init_type = sem_get_node_type(ctx, node->initializer);
        
        if (init_type.base == TYPE_VOID && init_type.ptr_depth == 0 && !init_type.is_func_ptr) {
            sem_error(ctx, (ASTNode*)node, "Cannot use expression of type 'void' to initialize variable '%s'", node->name);
        }

        int expr_tainted = sem_get_node_tainted(ctx, node->initializer);
        if (node->is_pristine && expr_tainted) {
            node->is_pristine = false;
        }

        if (node->var_type.base == TYPE_AUTO) {
            if (init_type.base == TYPE_UNKNOWN) {
                sem_error(ctx, (ASTNode*)node, "Cannot infer type for variable '%s' (unknown initializer type)", node->name);
            } else if (init_type.base == TYPE_VOID && init_type.ptr_depth == 0 && !init_type.is_func_ptr) {
                sem_error(ctx, (ASTNode*)node, "Cannot infer type 'void' for variable '%s'", node->name);
            } else {
                node->var_type = init_type; 
            }
        } 
        else {
            int is_stack_ctor = (node->var_type.base == TYPE_CLASS && node->var_type.ptr_depth == 0 && init_type.base == TYPE_CLASS && init_type.ptr_depth == 1 && node->var_type.class_name && init_type.class_name && strcmp(node->var_type.class_name, init_type.class_name) == 0);
            if (!sem_types_are_compatible(ctx,node->var_type, init_type) && !is_stack_ctor) {
                char *t1 = sem_type_to_str(node->var_type);
                char *t2 = sem_type_to_str(init_type);
                sem_error(ctx, (ASTNode*)node, "Type mismatch in declaration of '%s'. Expected '%s', got '%s'", node->name, t1, t2);
            } else {
                // TODO assign a proper implicit casting
                sem_check_implicit_cast(ctx, (ASTNode*)node, node->var_type, init_type);
            }
        }
    } else {
        if (node->var_type.base == TYPE_AUTO) {
            sem_error(ctx, (ASTNode*)node, "Variable '%s' declared 'let' but has no initializer", node->name);
        }
    }

    if (register_sym) {
        if (!ctx->settings.replace_variable && lookup_local_symbol(ctx, node->name)) {
            sem_error(ctx, (ASTNode*)node, "Redeclaration of variable '%s' in the same scope", node->name);
        } else {
            SemScope *shadow_scope = NULL;
            SemSymbol *shadow = sem_symbol_lookup(ctx, node->name, &shadow_scope);
            if (shadow) {
                if (shadow->inner_scope == ctx->global_scope) {
                    sem_info(ctx, (ASTNode*)node, "Shadowing global variable '%s'", node->name);
                } 
                else if (shadow_scope && shadow_scope->is_class_scope) {
                    sem_info(ctx, (ASTNode*)node, "Shadowing class member '%s'", node->name);
                }
                else {
                    sem_info(ctx, (ASTNode*)node, "Shadowing variable '%s' from outer scope", node->name);
                }
            }

            SemSymbol *sym = sem_symbol_add(ctx, node->name, SYM_VAR, node->var_type);
            sym->is_mutable = node->is_mutable; 
            sym->is_pure = node->is_pure;
            sym->must_pure = node->has_explicit_pure;
            sym->is_pristine = node->is_pristine;
            sym->must_pristine = node->has_explicit_pristine;            
      
            int is_global = (ctx->current_scope == ctx->global_scope);
            if (node->initializer || is_global || node->base.type == NODE_VAR_DECL) {
                 sym->is_initialized = true;
            } else {
                 sym->is_initialized = false;
            }
        }
    } else {
        SemSymbol *sym = lookup_local_symbol(ctx, node->name);
        if (sym) {
            sym->type = node->var_type;
            sym->is_mutable = node->is_mutable;
            sym->is_pure = node->is_pure;
            sym->must_pure = node->has_explicit_pure;
            sym->is_pristine = node->is_pristine;
            sym->must_pristine = node->has_explicit_pristine;
            if (node->initializer) sym->is_initialized = true;
        }
    }

    if (ctx->current_scope && ctx->current_scope->is_class_scope && node->var_type.base == TYPE_CLASS && node->var_type.class_name) {
        SemSymbol *type_sym = sem_symbol_lookup(ctx, node->var_type.class_name, NULL);
        if (type_sym && type_sym->kind == SYM_CLASS) {
            if (type_sym->is_has_a == HAS_A_INERT) {
                sem_error(ctx, (ASTNode*)node, "Class '%s' is inert, thus cannot be implicitly composed as field '%s'", type_sym->name, node->name);
            }
            type_sym->is_used_as_composition = true;
        }
    }
}

static bool sem_is_lvalue_mutable(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return true;
    if (node->type == NODE_VAR_REF) {
        VarRefNode *vr = (VarRefNode*)node;
        SemSymbol *sym = sem_symbol_lookup(ctx, vr->name, NULL);
        if (sym && !sym->is_mutable) return false;
        return true;
    } else if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccessNode *ma = (MemberAccessNode*)node;
        return sem_is_lvalue_mutable(ctx, ma->object);
    } else if (node->type == NODE_INDEX_ACCESS) {
        IndexAccessNode *aa = (IndexAccessNode*)node;
        return sem_is_lvalue_mutable(ctx, aa->target);
    } else if (node->type == NODE_UNARY_OP) {
        // e.g. pointer dereference `*ptr`
        return true; 
    }
    return true;
}

void sem_check_assign(SemanticCtx *ctx, AssignNode *node) {
    sem_check_expr(ctx, node->value);
    VarType rhs_type = sem_get_node_type(ctx, node->value);
    VarType lhs_type;
    int expr_tainted = sem_get_node_tainted(ctx, node->value);
   
    if (rhs_type.base == TYPE_VOID && rhs_type.ptr_depth == 0) {
        sem_error(ctx, (ASTNode*)node, "Cannot assign value of type 'void' to variable");
    }

    if (ctx->current_func_sym && ctx->current_func_sym->is_pure && node->name) {
        SemScope *scope = NULL;
        SemSymbol *sym = sem_symbol_lookup(ctx, node->name, &scope);
        if (sym && scope == ctx->global_scope) {
            sem_error(ctx, (ASTNode*)node, "Pure function '%s' cannot modify global variable '%s'", ctx->current_func_sym->name, sym->name);
        }
    }

    if (node->name) {
        SemScope *found = NULL;
        SemSymbol *sym = sem_symbol_lookup(ctx, node->name, &found);
        int implicit_this = 0;
        
        if (sym && found && found->is_class_scope) {
            implicit_this = 1;
        } else if (!sym) {
            SemSymbol *this_sym = sem_symbol_lookup(ctx, "this", NULL);
            if (this_sym && this_sym->type.base == TYPE_CLASS && this_sym->type.class_name) {
                SemSymbol *class_sym = sem_symbol_lookup(ctx, this_sym->type.class_name, NULL);
                if (class_sym && class_sym->inner_scope) {
                    SemScope *old_scope = ctx->current_scope;
                    ctx->current_scope = class_sym->inner_scope;
                    sym = sem_symbol_lookup(ctx, node->name, NULL);
                    ctx->current_scope = old_scope;
                    if (sym) implicit_this = 1;
                }
            }
        }

        
        if (!sym) {
            if (ctx->settings.implicit_let) {
                node->is_implicit_let = true;
                sym = sem_symbol_add(ctx, node->name, SYM_VAR, rhs_type);
                sym->is_mutable = true;
                sym->is_initialized = true;
                lhs_type = rhs_type;
            } else {
                sem_error(ctx, (ASTNode*)node, "Undefined variable '%s'", node->name);
                lhs_type = (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            }
        } else {
            if (!sym->is_mutable) {
                sem_error(ctx, (ASTNode*)node, "Cannot assign to immutable variable '%s'", node->name);
            }
            
            if (implicit_this) {
                VarRefNode *vr = arena_alloc_type(ctx->compiler_ctx->arena, VarRefNode);
                memset(vr, 0, sizeof(VarRefNode));
                vr->base.type = NODE_VAR_REF;
                vr->name = node->name;
                vr->is_class_member = 1;
                node->target = (ASTNode*)vr;
                node->name = NULL;
                lhs_type = sym->type;
            }

            
            if (sym->must_pristine && expr_tainted) {
                sem_error(ctx, (ASTNode*)node, "Cannot assign a tainted value to pristine variable '%s'", sym->name);
            }
            
            if (node->op != TOKEN_ASSIGN) {
                if (sym->kind == SYM_VAR && !sym->is_initialized) {
                    sem_error(ctx, (ASTNode*)node, "Use of uninitialized variable '%s' in compound assignment", node->name);
                }
            }

            if (ctx->settings.replace_variable && node->op == TOKEN_ASSIGN && !node->index) {
                node->is_implicit_let = true;
                sym = sem_symbol_add(ctx, node->name, SYM_VAR, rhs_type);
                sym->is_mutable = true;
                sym->is_initialized = true;
            }
            
            lhs_type = sym->type;
            
            if (node->index) {
                sem_check_expr(ctx, node->index);
                VarType idx_t = sem_get_node_type(ctx, node->index);
                if (!is_integer(idx_t)) {
                    sem_error(ctx, node->index, "Array index must be an integer");
                }
                
                if (lhs_type.array_size > 0) lhs_type.array_size = 0;
                else if (lhs_type.ptr_depth > 0) lhs_type.ptr_depth--;
                else {
                    sem_error(ctx, (ASTNode*)node, "Cannot index into non-array variable '%s'", node->name);
                }
                
                if (sym->kind == SYM_VAR && !sym->is_initialized) {
                    sem_error(ctx, (ASTNode*)node, "Use of uninitialized array '%s'", node->name);
                }
            } else {
                if (node->op == TOKEN_ASSIGN) {
                    sym->is_initialized = true;
                }
            }
        }
    } else {
        sem_check_expr(ctx, node->target);
        lhs_type = sem_get_node_type(ctx, node->target);
        
        if (!sem_is_lvalue_mutable(ctx, node->target)) {
            sem_error(ctx, (ASTNode*)node, "Cannot assign to immutable variable/member");
        }
    }
    
    if (node->op != TOKEN_ASSIGN) {
        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", TOKEN_INFMUT, node->op);
        
        SemSymbol *sym = NULL;
        int is_method = 0;
        if (lhs_type.base == TYPE_CLASS && lhs_type.ptr_depth == 0 && lhs_type.class_name) {
            SemSymbol *class_sym = sem_symbol_lookup(ctx, lhs_type.class_name, NULL);
            if (class_sym && class_sym->inner_scope) {
                SemSymbol *s = class_sym->inner_scope->symbols;
                while (s) {
                    if (strcmp(s->name, name_buf) == 0) { sym = s; is_method = 1; break; }
                    s = s->next;
                }
            }
        }
        if (!sym) {
            sym = sem_symbol_lookup(ctx, name_buf, NULL);
        }
        
        if (sym && sym->kind == SYM_FUNC) {
            if (is_method) {
                ASTNode *method_args = node->value;
                ASTNode *old_next = method_args->next;
                method_args->next = NULL;
                SemSymbol *resolved = sem_resolve_overload(ctx, &method_args, NULL, sym, NULL);
                method_args->next = old_next;
                if (resolved) {
                    node->overloaded_func_name = arena_strdup(ctx->compiler_ctx->arena, resolved->mangled_name ? resolved->mangled_name : resolved->name);
                    return;
                }
            } else {
                ASTNode *fake_args = node->target;
                if (!fake_args && node->name) {
                    VarRefNode *vr = arena_alloc_type(ctx->compiler_ctx->arena, VarRefNode);
                    memset(vr, 0, sizeof(VarRefNode));
                    vr->base.type = NODE_VAR_REF;
                    vr->name = node->name;
                    vr->base.line = node->base.line;
                    vr->base.col = node->base.col;
                    sem_set_node_type(ctx, (ASTNode*)vr, lhs_type);
                    fake_args = (ASTNode*)vr;
                }
                if (fake_args) {
                    UnaryOpNode *addr_of = arena_alloc_type(ctx->compiler_ctx->arena, UnaryOpNode);
                    memset(addr_of, 0, sizeof(UnaryOpNode));
                    addr_of->base.type = NODE_UNARY_OP;
                    addr_of->op = TOKEN_AND;
                    addr_of->operand = fake_args;
                    addr_of->base.line = node->base.line;
                    addr_of->base.col = node->base.col;
                    VarType ptr_type = lhs_type;
                    ptr_type.ptr_depth++;
                    sem_set_node_type(ctx, (ASTNode*)addr_of, ptr_type);
                    
                    addr_of->base.next = node->value;
                    node->value->next = NULL;
                    SemSymbol *resolved = sem_resolve_overload(ctx, (ASTNode**)&addr_of, NULL, sym, NULL);
                    if (resolved) {
                        node->overloaded_func_name = arena_strdup(ctx->compiler_ctx->arena, resolved->mangled_name ? resolved->mangled_name : resolved->name);
                        addr_of->base.next = NULL;
                        return;
                    }
                    addr_of->base.next = NULL;
                }
            }
        }
    }

    if (lhs_type.base != TYPE_UNKNOWN && rhs_type.base != TYPE_UNKNOWN) {
        if (!sem_types_are_compatible(ctx,lhs_type, rhs_type)) {
            // Check if lhs_type is a union, and if one of its fields is compatible
            int union_matched = 0;
            if (lhs_type.base == TYPE_CLASS && lhs_type.ptr_depth <= 1 && lhs_type.class_name) {
                SemSymbol *sym = sem_symbol_lookup(ctx, lhs_type.class_name, NULL);
                if (sym && sym->kind == SYM_CLASS && sym->is_union && sym->inner_scope) {
                    SemSymbol *f = sym->inner_scope->symbols;
                    while(f) {
                        if (f->kind == SYM_VAR && sem_types_are_compatible(ctx, f->type, rhs_type)) {
                            ASTNode *base_target = node->target;
                            if (!base_target && node->name) {
                                VarRefNode *vr = arena_alloc_type(ctx->compiler_ctx->arena, VarRefNode);
                                memset(vr, 0, sizeof(VarRefNode));
                                vr->base.type = NODE_VAR_REF;
                                vr->base.line = node->base.line;
                                vr->base.col = node->base.col;
                                vr->name = node->name;
                                sem_set_node_type(ctx, (ASTNode*)vr, lhs_type);
                                base_target = (ASTNode*)vr;
                                node->name = NULL;
                            }
                            
                            if (base_target) {
                                MemberAccessNode *ma = arena_alloc_type(ctx->compiler_ctx->arena, MemberAccessNode);
                                memset(ma, 0, sizeof(MemberAccessNode));
                                ma->base.type = NODE_MEMBER_ACCESS;
                                ma->base.line = node->base.line;
                                ma->base.col = node->base.col;
                                ma->object = base_target;
                                ma->member_name = arena_strdup(ctx->compiler_ctx->arena, f->name);
                                
                                sem_set_node_type(ctx, (ASTNode*)ma, f->type);
                                node->target = (ASTNode*)ma;
                                lhs_type = f->type; // Update lhs_type
                                union_matched = 1;
                            }
                            break;
                        }
                        f = f->next;
                    }
                }
            }
            
            if (!union_matched) {
                 char *t1 = sem_type_to_str(lhs_type);
                 char *t2 = sem_type_to_str(rhs_type);
                 sem_error(ctx, (ASTNode*)node, "Invalid assignment. Cannot assign '%s' to '%s'", t2, t1);
            } else {
                 sem_check_implicit_cast(ctx, (ASTNode*)node, lhs_type, rhs_type);
                 sem_set_node_type(ctx, (ASTNode*)node, lhs_type);
            }
    } else {
         sem_check_implicit_cast(ctx, (ASTNode*)node, lhs_type, rhs_type);
         sem_set_node_type(ctx, (ASTNode*)node, lhs_type);
    }

    // Propagate the attached error set from an errnum function call to the
    // assigned variable, so untaint/clean can later verify exhaustiveness.
    if (node->name) {
        SemSymbol *sym = sem_symbol_lookup(ctx, node->name, NULL);
        if (sym && sym->kind == SYM_VAR) {
            SemSymbol *err_sym = sem_get_errnum_func_sym(ctx, node->value);
            if (err_sym) {
                sym->has_errnum = err_sym->has_errnum;
                sym->num_err = err_sym->num_err;
                sym->err_names = err_sym->err_names;
            }

            // Exhaustiveness check for a `? ... ? ...` fallback chain.
            ASTNode *val = node->value;
            if (val && val->type == NODE_BINARY_OP &&
                (((BinaryOpNode*)val)->op == TOKEN_QUESTION ||
                 ((BinaryOpNode*)val)->op == TOKEN_QUESTION_QUESTION)) {
                // Walk down to the original tainted call at the bottom.
                ASTNode *c = val;
                while (c && c->type == NODE_BINARY_OP &&
                       (((BinaryOpNode*)c)->op == TOKEN_QUESTION ||
                        ((BinaryOpNode*)c)->op == TOKEN_QUESTION_QUESTION)) {
                    c = ((BinaryOpNode*)c)->left;
                }
                SemSymbol *src_err = sem_get_errnum_func_sym(ctx, c);
                if (src_err) {
                    ResidueCase *head = NULL;
                    ResidueCase **curr = &head;
                    int has_default = 0;
                    ASTNode *n = val;
                    while (n && n->type == NODE_BINARY_OP &&
                           (((BinaryOpNode*)n)->op == TOKEN_QUESTION ||
                            ((BinaryOpNode*)n)->op == TOKEN_QUESTION_QUESTION)) {
                        BinaryOpNode *bn = (BinaryOpNode*)n;
                        if (bn->cases) {
                            *curr = bn->cases;
                            curr = &bn->cases->next;
                        }
                        if (bn->cases && bn->cases->is_default) has_default = 1;
                        n = bn->left;
                    }
                    sem_check_residue_exhaustive(ctx, (ASTNode*)node, src_err, head, has_default);
                }
            }
        }
    }
}
}

int is_numeric(VarType t) {
    return ((t.base >= TYPE_INT && t.base <= TYPE_LONG_DOUBLE) || t.base == TYPE_ENUM) && t.ptr_depth == 0;
}

int is_integer(VarType t) {
    return ((t.base >= TYPE_INT && t.base <= TYPE_CHAR) || t.base == TYPE_ENUM) && t.ptr_depth == 0;
}

int is_bool(VarType t) {
    return (t.base == TYPE_BOOL && t.ptr_depth == 0);
}

int is_pointer(VarType t) {
    return t.ptr_depth > 0 || t.array_size > 0 || (t.base == TYPE_CLASS && t.class_name && (strcmp(t.class_name, "string") == 0 || strcmp(t.class_name, "vector") == 0)) || t.is_func_ptr;
}

