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

void sem_insert_implicit_cast(SemanticCtx *ctx, ASTNode **node_ptr, VarType target_type) {
    if (!node_ptr || !*node_ptr) return;
    VarType current = sem_get_node_type(ctx, *node_ptr);
    
    if (current.base == target_type.base && current.ptr_depth == target_type.ptr_depth && current.vector_depth == target_type.vector_depth) return;
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
            sem_symbolic_func_def(ctx, node);
        }
        else if (node->type == NODE_VAR_DECL) {
            sem_symbolic_var_decl(ctx, node);
        }
        else if (node->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)node;
            VarType type_class = {TYPE_CLASS, 0, 0, arena_strdup(ctx->compiler_ctx->arena, cn->name), 0, 0, NULL, NULL, 0, 0, 0, cn->is_tainted, 0};
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
        else if (node->type == NODE_TRAIT) {
            // TODO implement thsi
        }
        else if (node->type == NODE_IMPL) {
      
        }
        else if (node->type == NODE_STRUCT) {

        }
        else if (node->type == NODE_ENUM) {
            sem_symbolic_node_enum(ctx, node);
        }
        else if (node->type == NODE_NAMESPACE) {
            sem_symbolic_namespace(ctx, node);
        }
        else if (node->type == NODE_COMPOUND) {
            CompoundNode *cn = (CompoundNode*)node;
            char *template_name = NULL;
            if (cn->body && cn->body->type == NODE_FUNC_DEF) {
                template_name = ((FuncDefNode*)cn->body)->name;
            } else if (cn->body && cn->body->type == NODE_CLASS) {
                template_name = ((ClassNode*)cn->body)->name;
            }
            if (template_name) {
                VarType type_tmpl = {TYPE_UNKNOWN};
                SemSymbol *sym = sem_symbol_add(ctx, template_name, SYM_TEMPLATE, type_tmpl);
                sym->template_node = cn;
            }
        }
        node = node->next;
    }
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
        }
    }
    
    if (node->name) {
        sym = sem_symbol_lookup(ctx, node->name, NULL);
    }
    if (!sym) {
        sem_error(ctx, (ASTNode*)node, "Undefined function or class '%s'", node->name);
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
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

    if (sym->kind == SYM_CLASS) {
        VarType instance = {TYPE_CLASS, 1, 0, arena_strdup(ctx->compiler_ctx->arena, sym->name), 0, 0, NULL, NULL, 0, 0, 0, 0}; 
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
        if (!ctor_found) {
             // Basic validation for implicit constructor: should match number of fields
             // (This is tricky because fields might be inherited)
        }
    }
 else if (sym->kind == SYM_FUNC && sym->is_flux) {

        // Rewrite flux generator return type dynamically for iterators to intercept!
        char buf[256];
        snprintf(buf, sizeof(buf), "FluxCtx_%s", sym->name);
        VarType flux_type = {TYPE_CLASS, 1, 0, arena_strdup(ctx->compiler_ctx->arena, buf), 0, 0, NULL, NULL, 0, 0, 0, 0};
        flux_type.fp_ret_type = arena_alloc_type(ctx->compiler_ctx->arena, VarType);
        *flux_type.fp_ret_type = sym->type; // Save underlying yield type
        sem_set_node_type(ctx, (ASTNode*)node, flux_type);
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

    
    if (node->op == TOKEN_QUESTION) {
        sem_set_node_tainted(ctx, (ASTNode*)node, 0); // Result is pristine!
    } else if (sem_get_node_tainted(ctx, node->left) || sem_get_node_tainted(ctx, node->right)) {
        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
    } else if (node->op == TOKEN_SLASH) {
        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
    }

    if (l.base == TYPE_UNKNOWN || r.base == TYPE_UNKNOWN) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    if ((l.base == TYPE_VOID && l.ptr_depth == 0) || (r.base == TYPE_VOID && r.ptr_depth == 0)) {
        sem_error(ctx, (ASTNode*)node, "Operand of binary expression cannot be 'void'");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }
    
    if (node->op == TOKEN_AND_AND || node->op == TOKEN_OR_OR) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_BOOL, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }
    
    if (node->op == TOKEN_QUESTION) {
        // Fallback uses the type of the left hand side
        sem_set_node_type(ctx, (ASTNode*)node, l);
        return;
    }
   
    // todo fix this casting!
    if (node->op == TOKEN_EQ || node->op == TOKEN_NEQ || 
        node->op == TOKEN_LT || node->op == TOKEN_GT || 
        node->op == TOKEN_LTE || node->op == TOKEN_GTE) {
        
        if (is_numeric(l) && is_numeric(r)) {
            VarType target_type = {TYPE_INT, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            if (l.base == TYPE_LONG_DOUBLE || r.base == TYPE_LONG_DOUBLE) target_type.base = TYPE_LONG_DOUBLE;
            else if (l.base == TYPE_DOUBLE || r.base == TYPE_DOUBLE) target_type.base = TYPE_DOUBLE;
            else if (l.base == TYPE_FLOAT || r.base == TYPE_FLOAT) target_type.base = TYPE_FLOAT;
            else if (l.base == TYPE_LONG || r.base == TYPE_LONG) target_type.base = TYPE_LONG;
            else if (l.base == TYPE_UNSIGNED_INT || r.base == TYPE_UNSIGNED_INT) target_type.base = TYPE_UNSIGNED_INT;
            if (l.is_tainted || r.is_tainted) target_type.is_tainted = 1;
            sem_insert_implicit_cast(ctx, &node->left, target_type);
            sem_insert_implicit_cast(ctx, &node->right, target_type);
        }
        
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_BOOL, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }
    
    if (is_numeric(l) && is_numeric(r)) {
        VarType target_type = {TYPE_INT, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
        
        if (l.base == TYPE_LONG_DOUBLE || r.base == TYPE_LONG_DOUBLE) target_type.base = TYPE_LONG_DOUBLE;
        else if (l.base == TYPE_DOUBLE || r.base == TYPE_DOUBLE) target_type.base = TYPE_DOUBLE;
        else if (l.base == TYPE_FLOAT || r.base == TYPE_FLOAT) target_type.base = TYPE_FLOAT;
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
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
         }
    }
    else {
        sem_error(ctx, (ASTNode*)node, "Invalid operands for binary operator");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
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
            VarType size_type = {TYPE_UNSIGNED_LONG, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
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
        case NODE_MEMBER_ACCESS: sem_check_member_access(ctx, (MemberAccessNode*)node); break;
        case NODE_ARRAY_ACCESS: {
            sem_check_array_access(ctx, node);
            break;
        }
        case NODE_CAST: {
            CastNode *cn = (CastNode*)node;
            sem_check_expr(ctx, cn->operand);
            
            if (sem_get_node_tainted(ctx, cn->operand)) {
                sem_set_node_tainted(ctx, node, 1);
            }
            
            VarType op_t = sem_get_node_type(ctx, cn->operand);
            if (op_t.base == TYPE_VOID && op_t.ptr_depth == 0) {
                sem_error(ctx, node, "Cannot cast 'void' value");
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
            VarType elem_type = {TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
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
                t = sym ? sym->type : (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            } else {
                t = sem_get_node_type(ctx, an->target);
            }
            sem_set_node_type(ctx, node, t);
            break;
        }
        case NODE_TRAIT_ACCESS: {
            TraitAccessNode *ta = (TraitAccessNode*)node;
            sem_check_expr(ctx, ta->object);
            if (sem_get_node_tainted(ctx, ta->object)) sem_set_node_tainted(ctx, node, 1);
            VarType res = {TYPE_CLASS, 1, 0, arena_strdup(ctx->compiler_ctx->arena, ta->trait_name), 0, 0, NULL, NULL, 0, 0, 0, 0};
            sem_set_node_type(ctx, node, res);
            break;
        }
        case NODE_TYPEOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                sem_check_expr(ctx, sn->operand);
            }
            sem_set_node_type(ctx, node, (VarType){ .base = TYPE_CLASS, .class_name = (char*)"string" });
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
                            ln->var_type.base = TYPE_CLASS;
                            ln->var_type.class_name = arena_strdup(ctx->compiler_ctx->arena, "string");
                            ln->var_type.ptr_depth = 0;
                            ln->var_type.array_size = 0;
                            ln->val.str_val = arena_strdup(ctx->compiler_ctx->arena, s->name);
                            *curr = (ASTNode*)ln;
                            curr = &(*curr)->next;
                            count++;
                        }
                        s = s->next;
                    }
                }
            }
            
            // If none found, we still need an empty array, but we can't type check empty array easily.
            // Well, let's just create an empty array literal.
            node->type = NODE_ARRAY_LIT;
            ArrayLitNode *an = (ArrayLitNode*)node;
            an->elements = head;
            
            VarType arr_t = { .base = TYPE_CLASS, .class_name = arena_strdup(ctx->compiler_ctx->arena, "string"), .ptr_depth = 0, .array_size = count };
            sem_set_node_type(ctx, node, arr_t);
            break;
        }
        case NODE_TEMPLATE_INSTANTIATION: {
            TemplateInstNode *ti = (TemplateInstNode*)node;
            if (ti->target->type != NODE_VAR_REF) {
                sem_error(ctx, node, "Expected identifier for template instantiation");
                break;
            }
            VarRefNode *vr = (VarRefNode*)ti->target;
            SemSymbol *sym = sem_symbol_lookup(ctx, vr->name, NULL);
            if (!sym || sym->kind != SYM_TEMPLATE) {
                // Fallback to array access / component access!
                if (ti->num_template_types == 1 && ti->template_types[0].base == TYPE_CLASS) {
                    VarRefNode *index_vr = arena_alloc(ctx->compiler_ctx->arena, sizeof(VarRefNode));
                    index_vr->base.type = NODE_VAR_REF;
                    index_vr->name = arena_strdup(ctx->compiler_ctx->arena, ti->template_types[0].class_name);
                    index_vr->base.line = node->line;
                    index_vr->base.col = node->col;
                    
                    ArrayAccessNode *aa = (ArrayAccessNode*)node;
                    aa->base.type = NODE_ARRAY_ACCESS;
                    // aa->target is already vr, so we don't need to change it
                    aa->index = (ASTNode*)index_vr;
                    
                    sem_check_expr(ctx, node); // Check again as array access
                    break;
                }
                
                sem_error(ctx, node, "'%s' is not a template", vr->name);
                break;
            }
            CompoundNode *cn = sym->template_node;
            if (ti->num_template_types != cn->num_type_params) {
                sem_error(ctx, node, "Template '%s' expects %d types, got %d", vr->name, cn->num_type_params, ti->num_template_types);
                break;
            }
            
            // Generate mangled name
            char mangled[1024];
            snprintf(mangled, sizeof(mangled), "%s", vr->name);
            for (int i = 0; i < ti->num_template_types; i++) {
                char *t_str = sem_type_to_str(ti->template_types[i]);
                strncat(mangled, "_", sizeof(mangled) - strlen(mangled) - 1);
                strncat(mangled, t_str, sizeof(mangled) - strlen(mangled) - 1);
            }
            
            SemSymbol *inst_sym = sem_symbol_lookup(ctx, mangled, NULL);
            if (!inst_sym) {
                // Instantiate it
                ASTNode *cloned_body = ast_clone(ctx->compiler_ctx, cn->body, cn->type_params, ti->template_types, ti->num_template_types);
                
                // Change the name to mangled name
                if (cloned_body->type == NODE_FUNC_DEF) {
                    ((FuncDefNode*)cloned_body)->name = arena_strdup(ctx->compiler_ctx->arena, mangled);
                } else if (cloned_body->type == NODE_CLASS) {
                    ((ClassNode*)cloned_body)->name = arena_strdup(ctx->compiler_ctx->arena, mangled);
                }
                
                if (ctx->ast_tail) {
                    *ctx->ast_tail = cloned_body;
                    ctx->ast_tail = &cloned_body->next;
                }
                
                // Add to global AST? Just scan and check it now!
                sem_scan_top_level(ctx, cloned_body);
                sem_check_node(ctx, cloned_body);
                inst_sym = sem_symbol_lookup(ctx, mangled, NULL);
            }
            
            // Replace the current node with a VarRef to the mangled name, so codegen just calls the instantiated function/class
            // Wait, this is an expression! A template instantiation `map[int]` resolves to the function name itself.
            // So its type should be the type of `inst_sym`.
            if (inst_sym) {
                sem_set_node_type(ctx, node, inst_sym->type);
                // Also update the target so Codegen emits the mangled name
                vr->name = arena_strdup(ctx->compiler_ctx->arena, mangled);
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
            if (pn->msg->type == NODE_VAR_REF) {
                VarRefNode *var = (VarRefNode*)pn->msg;
                // It's an error identifier!
                if (!hashmap_get(&ctx->compiler_ctx->error_table, var->name)) {
                    hashmap_put(&ctx->compiler_ctx->error_table, strdup(var->name), (void*)1);
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
        case NODE_WASH: {
            WashNode *wn = (WashNode*)node;
            sem_check_expr(ctx, wn->target);
            SemSymbol *target_sym = NULL;
            if (wn->target->type == NODE_VAR_REF) {
                target_sym = sem_symbol_lookup(ctx, ((VarRefNode*)wn->target)->name, NULL);
            }
            if (wn->wash_type == WASH_TYPE_UNTAINT) { 
                sem_scope_enter(ctx, 0, (VarType){0});
                
                if (wn->err_name) {
                    VarType err_type = {TYPE_INT, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0}; 
                    SemSymbol *err_sym = sem_symbol_add(ctx, wn->err_name, SYM_VAR, err_type);
                    err_sym->is_initialized = 1;
                }
                
                sem_check_block(ctx, wn->body);
                sem_scope_exit(ctx);
                
                if (target_sym) {
                    target_sym->is_pristine = 1; 
                }
            } else if (wn->wash_type == WASH_TYPE_WASH || wn->wash_type == WASH_TYPE_CLEAN) { 
                sem_scope_enter(ctx, 0, (VarType){0});
                
                if (wn->err_name) {
                    VarType err_type = {TYPE_INT, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0}; 
                    SemSymbol *err_sym = sem_symbol_add(ctx, wn->err_name, SYM_VAR, err_type);
                    err_sym->is_initialized = 1;
                }
                
                int old_pristine = target_sym ? target_sym->is_pristine : 0;
                if (target_sym) target_sym->is_pristine = 1;
                
                sem_check_block(ctx, wn->body);
                
                if (target_sym && wn->wash_type == WASH_TYPE_WASH) target_sym->is_pristine = old_pristine;
                
                sem_scope_exit(ctx);
                
                if (wn->else_body) {
                    ctx->in_wash_block++;
                    sem_scope_enter(ctx, 0, (VarType){0});
                    
                    if (wn->else_body->type == NODE_WASH || wn->else_body->type == NODE_IF) {
                        sem_check_node(ctx, wn->else_body);
                    } else {
                        sem_check_block(ctx, wn->else_body);
                    }
                    
                    sem_scope_exit(ctx);
                    ctx->in_wash_block--;
                }
            }
            break;
        }
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
        case NODE_CALL:
        case NODE_METHOD_CALL:
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
        VECTOR_LIT
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
        SemSymbol *sym = sem_symbol_lookup(ctx, cn->name, NULL);
        if (sym && sym->inner_scope) {
            SemScope *old = ctx->current_scope;
            ctx->current_scope = sym->inner_scope;
            
            ASTNode *mem = cn->members;
            while(mem) {
                if (mem->type == NODE_FUNC_DEF) sem_check_func_def(ctx, (FuncDefNode*)mem);
                else if (mem->type == NODE_VAR_DECL) sem_check_var_decl(ctx, (VarDeclNode*)mem, 0); 
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
            sem_check_block(ctx, ns->body);
            ctx->current_scope = old;
        }
    }
    else if (node->type == NODE_VAR_DECL) {
        sem_check_var_decl(ctx, (VarDeclNode*)node, 1);
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
    
    // Find matching overload (exact types or compatible implicit cast)
    while (sym) {
        if (sym->param_count == arg_count || sym->is_variadic) {
            int match = 1;
            int exact_matches = 0;
            curr_arg = *args;
            Parameter *curr_para = sym->params;
            while(curr_arg && curr_para) {
                VarType arg_t = sem_get_node_type(ctx, curr_arg);
                if (!sem_types_are_compatible(ctx, curr_para->type, arg_t)) {
                    match = 0;
                    break;
                }
                if (curr_para->type.base == arg_t.base && curr_para->type.ptr_depth == arg_t.ptr_depth) {
                    exact_matches++;
                }
                curr_arg = curr_arg->next;
                curr_para = curr_para->next;
            }
            if (match) {
                int score = exact_matches;
                if (score > best_score) {
                    best_score = score;
                    best_match = sym;
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
