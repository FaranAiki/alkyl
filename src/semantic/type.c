#include "semantic.h"

// Helper to check for implicit cast between string and char* and emit info
void sem_check_implicit_cast(SemanticCtx *ctx, ASTNode *node, VarType dest, VarType src) {
    int dest_is_str = (dest.base == TYPE_STRING && dest.ptr_depth == 0);
    int src_is_char = (src.base == TYPE_CHAR && (src.ptr_depth > 0 || src.array_size > 0));
    
    int dest_is_char = (dest.base == TYPE_CHAR && (dest.ptr_depth > 0 || dest.array_size > 0));
    int src_is_str = (src.base == TYPE_STRING && src.ptr_depth == 0);
    
    if (dest_is_str && src_is_char) {
        sem_info(ctx, node, "Implicit cast from 'char%s' to 'string'", (src.array_size > 0) ? "[]" : "*");
    } else if (dest_is_char && src_is_str) {
        sem_info(ctx, node, "Implicit cast from 'string' to 'char%s'", (dest.array_size > 0) ? "[]" : "*");
    }
}

void sem_check_var_decl(SemanticCtx *ctx, VarDeclNode *node, int register_sym) {
    // 1. Check Initializer
    if (node->initializer) {
        sem_check_expr(ctx, node->initializer);
        VarType init_type = sem_get_node_type(ctx, node->initializer);
        
        // 2. Inference (let / auto)
        if (node->var_type.base == TYPE_AUTO) {
            if (init_type.base == TYPE_UNKNOWN) {
                sem_error(ctx, (ASTNode*)node, "Cannot infer type for variable '%s' (unknown initializer type)", node->name);
            } else if (init_type.base == TYPE_VOID) {
                sem_error(ctx, (ASTNode*)node, "Cannot infer type 'void' for variable '%s'", node->name);
            } else {
                node->var_type = init_type; 
            }
        } 
        // 3. Compatibility Check
        else {
            if (!sem_types_are_compatible(node->var_type, init_type)) {
                // sem_type_to_str uses a rotating buffer now, so calling it twice is safe
                char *t1 = sem_type_to_str(node->var_type);
                char *t2 = sem_type_to_str(init_type);
                sem_error(ctx, (ASTNode*)node, "Type mismatch in declaration of '%s'. Expected '%s', got '%s'", node->name, t1, t2);
            } else {
                // Check for implicit cast info (string <-> char*)
                sem_check_implicit_cast(ctx, (ASTNode*)node, node->var_type, init_type);
            }
        }
    } else {
        if (node->var_type.base == TYPE_AUTO) {
            sem_error(ctx, (ASTNode*)node, "Variable '%s' declared 'let' but has no initializer", node->name);
        }
    }

    // 4. Register Symbol (or update if already exists from Scan)
    if (register_sym) {
        if (lookup_local_symbol(ctx, node->name)) {
            sem_error(ctx, (ASTNode*)node, "Redeclaration of variable '%s' in the same scope", node->name);
        } else {
            sem_symbol_add(ctx, node->name, SYM_VAR, node->var_type);
        }
    } else {
        // If we are checking a global or member, it exists, but we might need to update TYPE_AUTO resolved type
        SemSymbol *sym = lookup_local_symbol(ctx, node->name);
        if (sym) {
            sym->type = node->var_type;
        }
    }
}

void sem_check_assign(SemanticCtx *ctx, AssignNode *node) {
    sem_check_expr(ctx, node->value);
    VarType rhs_type = sem_get_node_type(ctx, node->value);
    VarType lhs_type;

    if (node->name) {
        SemSymbol *sym = sem_symbol_lookup(ctx, node->name);
        if (!sym) {
            sem_error(ctx, (ASTNode*)node, "Undefined variable '%s'", node->name);
            lhs_type = (VarType){TYPE_UNKNOWN};
        } else {
            if (!sym->is_mutable) {
                sem_error(ctx, (ASTNode*)node, "Cannot assign to immutable variable '%s'", node->name);
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
            }
        }
    } else {
        sem_check_expr(ctx, node->target);
        lhs_type = sem_get_node_type(ctx, node->target);
    }

    if (lhs_type.base != TYPE_UNKNOWN && rhs_type.base != TYPE_UNKNOWN) {
        if (!sem_types_are_compatible(lhs_type, rhs_type)) {
             char *t1 = sem_type_to_str(lhs_type);
             char *t2 = sem_type_to_str(rhs_type);
             sem_error(ctx, (ASTNode*)node, "Invalid assignment. Cannot assign '%s' to '%s'", t2, t1);
        } else {
             sem_check_implicit_cast(ctx, (ASTNode*)node, lhs_type, rhs_type);
        }
    }
}

int is_numeric(VarType t) {
    return (t.base >= TYPE_INT && t.base <= TYPE_LONG_DOUBLE && t.ptr_depth == 0);
}

int is_integer(VarType t) {
    return (t.base >= TYPE_INT && t.base <= TYPE_CHAR && t.ptr_depth == 0);
}

int is_bool(VarType t) {
    return (t.base == TYPE_BOOL && t.ptr_depth == 0);
}

int is_pointer(VarType t) {
    return t.ptr_depth > 0 || t.array_size > 0 || t.base == TYPE_STRING || t.is_func_ptr;
}
