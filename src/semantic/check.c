#include "semantic.h"
#include "../diagnostic/diagnostic.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// --- Forward Declarations ---
void sem_check_var_decl(SemanticCtx *ctx, VarDeclNode *node, int register_sym);
void sem_check_stmt(SemanticCtx *ctx, ASTNode *node);

// --- Internal Helper: Local Lookup ---
// Used to detect redeclarations in the same scope
SemSymbol* lookup_local_symbol(SemanticCtx *ctx, const char *name) {
    if (!ctx->current_scope) return NULL;
    SemSymbol *sym = ctx->current_scope->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) return sym;
        sym = sym->next;
    }
    return NULL;
}

void sem_register_builtins(SemanticCtx *ctx) {
    VarType int_t = {TYPE_INT, 0, NULL, 0, 0};
    sem_symbol_add(ctx, "printf", SYM_FUNC, int_t);
    sem_symbol_add(ctx, "print", SYM_FUNC, int_t);
    
    VarType void_ptr = {TYPE_VOID, 1, NULL, 0, 0};
    sem_symbol_add(ctx, "malloc", SYM_FUNC, void_ptr);
    sem_symbol_add(ctx, "alloc", SYM_FUNC, void_ptr);
    
    VarType void_t = {TYPE_VOID, 0, NULL, 0, 0};
    sem_symbol_add(ctx, "free", SYM_FUNC, void_t);

    VarType str_t = {TYPE_STRING, 0, NULL, 0, 0};
    sem_symbol_add(ctx, "input", SYM_FUNC, str_t);
    
    sem_symbol_add(ctx, "exit", SYM_FUNC, void_t);
}

// --- Standardized Reporting ---

void sem_error(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...) {
    ctx->error_count++;
    
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (ctx->current_source && node) {
        Lexer l;
        lexer_init(&l, ctx->current_source);
        l.filename = (char*)ctx->current_filename; // Pass filename to lexer for diagnostics
        
        Token t;
        t.line = node->line;
        t.col = node->col;
        t.type = TOKEN_UNKNOWN; 
        t.text = NULL;
        t.int_val = 0; 
        t.double_val = 0.0;
        
        report_error(&l, t, msg);
    } else {
        if (node) {
            fprintf(stderr, "[Semantic Error] Line %d, Col %d: %s\n", node->line, node->col, msg);
        } else {
            fprintf(stderr, "[Semantic Error] %s\n", msg);
        }
    }
}

void sem_info(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (ctx->current_source && node) {
        Lexer l;
        lexer_init(&l, ctx->current_source);
        l.filename = (char*)ctx->current_filename; // Pass filename to lexer for diagnostics
        
        Token t;
        t.line = node->line;
        t.col = node->col;
        t.type = TOKEN_UNKNOWN; 
        t.text = NULL;
        
        report_info(&l, t, msg);
    } else {
        fprintf(stderr, "[Semantic Info] %s\n", msg);
    }
}

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

// --- Type Helpers ---

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

// --- Pass 1: Scanning ---

void sem_scan_class_members(SemanticCtx *ctx, ClassNode *cn, SemSymbol *class_sym) {
    SemScope *class_scope = malloc(sizeof(SemScope));
    class_scope->symbols = NULL;
    class_scope->parent = ctx->current_scope; 
    class_scope->is_function_scope = 0;
    class_scope->expected_ret_type = (VarType){0};
    
    class_sym->inner_scope = class_scope;
    
    SemScope *old_scope = ctx->current_scope;
    ctx->current_scope = class_scope;
    
    ASTNode *mem = cn->members;
    while(mem) {
        if (mem->type == NODE_VAR_DECL) {
            VarDeclNode *vd = (VarDeclNode*)mem;
            // Class fields are symbols in the class scope
            sem_symbol_add(ctx, vd->name, SYM_VAR, vd->var_type);
        } else if (mem->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)mem;
            // Methods are symbols in the class scope
            sem_symbol_add(ctx, fd->name, SYM_FUNC, fd->ret_type);
        }
        mem = mem->next;
    }
    
    ctx->current_scope = old_scope;
}

void sem_scan_top_level(SemanticCtx *ctx, ASTNode *node) {
    while (node) {
        if (node->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)node;
            sem_symbol_add(ctx, fd->name, SYM_FUNC, fd->ret_type);
        }
        else if (node->type == NODE_VAR_DECL) {
            // Global variables
            VarDeclNode *vd = (VarDeclNode*)node;
            sem_symbol_add(ctx, vd->name, SYM_VAR, vd->var_type);
        }
        else if (node->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)node;
            SemSymbol *sym = sem_symbol_add(ctx, cn->name, SYM_CLASS, (VarType){TYPE_CLASS, 0, strdup(cn->name)});
            if (cn->parent_name) {
                sym->parent_name = strdup(cn->parent_name);
            }
            sem_scan_class_members(ctx, cn, sym);
        }
        else if (node->type == NODE_ENUM) {
            EnumNode *en = (EnumNode*)node;
            sem_symbol_add(ctx, en->name, SYM_ENUM, (VarType){TYPE_INT, 0, NULL});
            // Note: Enum members could be registered as constants here
        }
        else if (node->type == NODE_NAMESPACE) {
            NamespaceNode *ns = (NamespaceNode*)node;
            SemSymbol *sym = sem_symbol_add(ctx, ns->name, SYM_NAMESPACE, (VarType){TYPE_VOID});
            
            SemScope *ns_scope = malloc(sizeof(SemScope));
            ns_scope->symbols = NULL;
            ns_scope->parent = ctx->current_scope;
            sym->inner_scope = ns_scope;
            
            SemScope *old = ctx->current_scope;
            ctx->current_scope = ns_scope;
            sem_scan_top_level(ctx, ns->body);
            ctx->current_scope = old;
        }
        node = node->next;
    }
}

// --- Pass 2: Checking ---

// register_sym: 1 for locals (add to scope), 0 for globals/members (already added in Scan)
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

void sem_check_call(SemanticCtx *ctx, CallNode *node) {
    SemSymbol *sym = sem_symbol_lookup(ctx, node->name);
    
    if (!sym) {
        sem_error(ctx, (ASTNode*)node, "Undefined function or class '%s'", node->name);
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
        return;
    }
    
    int arg_count = 0;
    ASTNode *arg = node->args;
    while(arg) {
        sem_check_expr(ctx, arg);
        arg = arg->next;
        arg_count++;
    }

    if (sym->kind == SYM_CLASS) {
        VarType instance = {TYPE_CLASS, 1, strdup(sym->name), 0, 0}; 
        sem_set_node_type(ctx, (ASTNode*)node, instance);
    } else {
        sem_set_node_type(ctx, (ASTNode*)node, sym->type);
    }
}

void sem_check_binary_op(SemanticCtx *ctx, BinaryOpNode *node) {
    sem_check_expr(ctx, node->left);
    sem_check_expr(ctx, node->right);
    
    VarType l = sem_get_node_type(ctx, node->left);
    VarType r = sem_get_node_type(ctx, node->right);
    
    if (l.base == TYPE_UNKNOWN || r.base == TYPE_UNKNOWN) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
        return;
    }
    
    if (node->op == TOKEN_AND_AND || node->op == TOKEN_OR_OR) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_BOOL});
        return;
    }
    
    if (node->op == TOKEN_EQ || node->op == TOKEN_NEQ || 
        node->op == TOKEN_LT || node->op == TOKEN_GT || 
        node->op == TOKEN_LTE || node->op == TOKEN_GTE) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_BOOL});
        return;
    }
    
    if (is_numeric(l) && is_numeric(r)) {
        if (l.base == TYPE_LONG_DOUBLE || r.base == TYPE_LONG_DOUBLE) 
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_LONG_DOUBLE});
        else if (l.base == TYPE_DOUBLE || r.base == TYPE_DOUBLE) 
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_DOUBLE});
        else if (l.base == TYPE_FLOAT || r.base == TYPE_FLOAT) 
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_FLOAT});
        else if (l.base == TYPE_LONG || r.base == TYPE_LONG) 
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_LONG});
        else 
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_INT});
    } 
    else if (is_pointer(l) && is_integer(r)) {
         sem_set_node_type(ctx, (ASTNode*)node, l);
    }
    else if (is_integer(l) && is_pointer(r)) {
         sem_set_node_type(ctx, (ASTNode*)node, r);
    }
    else if (l.base == TYPE_STRING || r.base == TYPE_STRING) {
         if (node->op == TOKEN_PLUS) 
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_STRING});
         else {
            sem_error(ctx, (ASTNode*)node, "Invalid operation on strings");
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
         }
    }
    else {
        sem_error(ctx, (ASTNode*)node, "Invalid operands for binary operator");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
    }
}

void sem_check_member_access(SemanticCtx *ctx, MemberAccessNode *node) {
    sem_check_expr(ctx, node->object);
    VarType obj_type = sem_get_node_type(ctx, node->object);
    
    if (obj_type.base == TYPE_UNKNOWN) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
        return;
    }
    
    if (obj_type.base == TYPE_CLASS && obj_type.class_name) {
        SemSymbol *class_sym = sem_symbol_lookup(ctx, obj_type.class_name);
        if (!class_sym || class_sym->kind != SYM_CLASS) {
            sem_error(ctx, (ASTNode*)node, "Type '%s' is not a class/struct", obj_type.class_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
            return;
        }
        
        SemSymbol *current_class = class_sym;
        int found = 0;
        
        while (current_class) {
            if (current_class->inner_scope) {
                SemSymbol *member = current_class->inner_scope->symbols;
                while (member) {
                    if (strcmp(member->name, node->member_name) == 0) {
                        sem_set_node_type(ctx, (ASTNode*)node, member->type);
                        found = 1;
                        goto done_search;
                    }
                    member = member->next;
                }
            }
            if (current_class->parent_name) {
                current_class = sem_symbol_lookup(ctx, current_class->parent_name);
            } else {
                current_class = NULL;
            }
        }
        
        done_search:
        if (!found) {
            sem_error(ctx, (ASTNode*)node, "Class '%s' has no member named '%s'", obj_type.class_name, node->member_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
        }
    } 
    else if (obj_type.base == TYPE_STRING && strcmp(node->member_name, "length") == 0) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_INT});
    }
    else {
        sem_error(ctx, (ASTNode*)node, "Cannot access member on non-class type");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
    }
}

void sem_check_method_call(SemanticCtx *ctx, MethodCallNode *node) {
    sem_check_expr(ctx, node->object);
    VarType obj_type = sem_get_node_type(ctx, node->object);
    
    if (obj_type.base == TYPE_UNKNOWN) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
        return;
    }

    if (obj_type.base == TYPE_CLASS && obj_type.class_name) {
        SemSymbol *class_sym = sem_symbol_lookup(ctx, obj_type.class_name);
        if (!class_sym || class_sym->kind != SYM_CLASS) {
            sem_error(ctx, (ASTNode*)node, "Type '%s' is not a class/struct", obj_type.class_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
            return;
        }
        
        SemSymbol *current_class = class_sym;
        int found = 0;
        
        while (current_class) {
            if (current_class->inner_scope) {
                SemSymbol *member = current_class->inner_scope->symbols;
                while (member) {
                    if (strcmp(member->name, node->method_name) == 0) {
                        if (member->kind == SYM_FUNC) {
                            sem_set_node_type(ctx, (ASTNode*)node, member->type); 
                            found = 1;
                        } 
                        else if (member->kind == SYM_VAR && member->type.is_func_ptr) {
                             sem_set_node_type(ctx, (ASTNode*)node, *member->type.fp_ret_type);
                             found = 1;
                        }

                        if (found) {
                            ASTNode *arg = node->args;
                            while(arg) {
                                sem_check_expr(ctx, arg);
                                arg = arg->next;
                            }
                            goto done_method_search;
                        }
                    }
                    member = member->next;
                }
            }
            if (current_class->parent_name) {
                current_class = sem_symbol_lookup(ctx, current_class->parent_name);
            } else {
                current_class = NULL;
            }
        }
        
        done_method_search:
        if (!found) {
             sem_error(ctx, (ASTNode*)node, "Method '%s' not found in class '%s'", node->method_name, obj_type.class_name);
             sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
        }
    } else {
        sem_error(ctx, (ASTNode*)node, "Cannot call method on non-class type");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN});
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
        case NODE_VAR_REF: {
            VarRefNode *ref = (VarRefNode*)node;
            SemSymbol *sym = sem_symbol_lookup(ctx, ref->name);
            if (sym) {
                sem_set_node_type(ctx, node, sym->type);
            } else {
                sem_error(ctx, node, "Undefined variable '%s'", ref->name);
                sem_set_node_type(ctx, node, (VarType){TYPE_UNKNOWN});
            }
            break;
        }
        case NODE_BINARY_OP: sem_check_binary_op(ctx, (BinaryOpNode*)node); break;
        case NODE_UNARY_OP: {
            UnaryOpNode *un = (UnaryOpNode*)node;
            sem_check_expr(ctx, un->operand);
            VarType t = sem_get_node_type(ctx, un->operand);
            
            if (un->op == TOKEN_AND) { 
                t.ptr_depth++;
            } else if (un->op == TOKEN_STAR) { 
                if (t.ptr_depth > 0) t.ptr_depth--;
                else sem_error(ctx, node, "Cannot dereference non-pointer");
            } else if (un->op == TOKEN_NOT) {
                t = (VarType){TYPE_BOOL};
            }
            sem_set_node_type(ctx, node, t);
            break;
        }
        case NODE_CALL: sem_check_call(ctx, (CallNode*)node); break;
        case NODE_MEMBER_ACCESS: sem_check_member_access(ctx, (MemberAccessNode*)node); break;
        case NODE_ARRAY_ACCESS: {
            ArrayAccessNode *aa = (ArrayAccessNode*)node;
            sem_check_expr(ctx, aa->target);
            sem_check_expr(ctx, aa->index);
            
            VarType t = sem_get_node_type(ctx, aa->target);
            if (t.array_size > 0) t.array_size = 0;
            else if (t.ptr_depth > 0) t.ptr_depth--;
            else {
                sem_error(ctx, node, "Type is not an array or pointer");
                t = (VarType){TYPE_UNKNOWN};
            }
            sem_set_node_type(ctx, node, t);
            break;
        }
        case NODE_CAST: {
            CastNode *cn = (CastNode*)node;
            sem_check_expr(ctx, cn->operand);
            sem_set_node_type(ctx, node, cn->var_type);
            break;
        }
        case NODE_METHOD_CALL: {
            sem_check_method_call(ctx, (MethodCallNode*)node);
            break;
        }
        case NODE_ARRAY_LIT: {
            ArrayLitNode *al = (ArrayLitNode*)node;
            ASTNode *el = al->elements;
            VarType elem_type = {TYPE_UNKNOWN};
            if (el) {
                sem_check_expr(ctx, el);
                elem_type = sem_get_node_type(ctx, el);
                el = el->next;
            }
            while(el) {
                sem_check_expr(ctx, el);
                el = el->next;
            }
            elem_type.ptr_depth++;
            sem_set_node_type(ctx, node, elem_type);
            break;
        }
        default: break;
    }
}

void sem_check_node(SemanticCtx *ctx, ASTNode *node);
void sem_check_block(SemanticCtx *ctx, ASTNode *block);

void sem_check_stmt(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_VAR_DECL: sem_check_var_decl(ctx, (VarDeclNode*)node, 1); break;
        case NODE_ASSIGN: sem_check_assign(ctx, (AssignNode*)node); break;
        case NODE_RETURN: {
            ReturnNode *rn = (ReturnNode*)node;
            if (rn->value) {
                sem_check_expr(ctx, rn->value);
                VarType val = sem_get_node_type(ctx, rn->value);
                if (ctx->current_scope->is_function_scope) {
                    if (!sem_types_are_compatible(ctx->current_scope->expected_ret_type, val)) {
                        sem_error(ctx, node, "Return type mismatch");
                    } else {
                         sem_check_implicit_cast(ctx, node, ctx->current_scope->expected_ret_type, val);
                    }
                }
            } else {
                 if (ctx->current_scope->is_function_scope && ctx->current_scope->expected_ret_type.base != TYPE_VOID) {
                     sem_error(ctx, node, "Function must return a value");
                 }
            }
            break;
        }
        case NODE_IF: {
            IfNode *ifn = (IfNode*)node;
            sem_check_expr(ctx, ifn->condition);
            
            // Enter Block Scope for Then
            sem_scope_enter(ctx, 0, (VarType){0});
            sem_check_block(ctx, ifn->then_body);
            sem_scope_exit(ctx);
            
            if (ifn->else_body) {
                // Enter Block Scope for Else
                sem_scope_enter(ctx, 0, (VarType){0});
                sem_check_block(ctx, ifn->else_body);
                sem_scope_exit(ctx);
            }
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
            ForInNode *fn = (ForInNode*)node;
            sem_check_expr(ctx, fn->collection);
            ctx->in_loop++;
            
            sem_scope_enter(ctx, 0, (VarType){0});
            VarType iter_type = {TYPE_AUTO}; 
            sem_symbol_add(ctx, fn->var_name, SYM_VAR, iter_type);
            
            sem_check_block(ctx, fn->body);
            sem_scope_exit(ctx);
            
            ctx->in_loop--;
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

void sem_check_func_def(SemanticCtx *ctx, FuncDefNode *node) {
    sem_scope_enter(ctx, 1, node->ret_type);
    
    if (node->class_name) {
        VarType this_type = {TYPE_CLASS, 1, strdup(node->class_name), 0, 0}; 
        sem_symbol_add(ctx, "this", SYM_VAR, this_type);
    }

    Parameter *p = node->params;
    while (p) {
        if (p->name) {
            sem_symbol_add(ctx, p->name, SYM_VAR, p->type);
        }
        p = p->next;
    }
    
    sem_check_block(ctx, node->body);
    sem_scope_exit(ctx);
}

void sem_check_node(SemanticCtx *ctx, ASTNode *node) {
    if (node->type == NODE_FUNC_DEF) sem_check_func_def(ctx, (FuncDefNode*)node);
    else if (node->type == NODE_CLASS) {
        ClassNode *cn = (ClassNode*)node;
        SemSymbol *sym = sem_symbol_lookup(ctx, cn->name);
        if (sym && sym->inner_scope) {
            SemScope *old = ctx->current_scope;
            ctx->current_scope = sym->inner_scope;
            
            ASTNode *mem = cn->members;
            while(mem) {
                if (mem->type == NODE_FUNC_DEF) sem_check_func_def(ctx, (FuncDefNode*)mem);
                // Check fields (don't register, already in scan) to verify initializers
                else if (mem->type == NODE_VAR_DECL) sem_check_var_decl(ctx, (VarDeclNode*)mem, 0); 
                mem = mem->next;
            }
            
            ctx->current_scope = old;
        }
    }
    else if (node->type == NODE_NAMESPACE) {
        NamespaceNode *ns = (NamespaceNode*)node;
        SemSymbol *sym = sem_symbol_lookup(ctx, ns->name);
        if (sym && sym->inner_scope) {
            SemScope *old = ctx->current_scope;
            ctx->current_scope = sym->inner_scope;
            sem_check_block(ctx, ns->body);
            ctx->current_scope = old;
        }
    }
    else if (node->type == NODE_VAR_DECL) {
        // Default dispatch for local variables inside blocks
        sem_check_var_decl(ctx, (VarDeclNode*)node, 1);
    }
    else {
        sem_check_stmt(ctx, node);
    }
}

int sem_check_program(SemanticCtx *ctx, ASTNode *root) {
    if (!root) return 0;
    
    sem_register_builtins(ctx);
    sem_scan_top_level(ctx, root);
    
    if (ctx->error_count > 0) return ctx->error_count;
    
    ASTNode *curr = root;
    while (curr) {
        if (curr->type == NODE_VAR_DECL) {
            // Check global var initializers (don't register, already scanned)
            sem_check_var_decl(ctx, (VarDeclNode*)curr, 0);
        } else {
            sem_check_node(ctx, curr);
        }
        curr = curr->next;
    }
    
    return ctx->error_count;
}
