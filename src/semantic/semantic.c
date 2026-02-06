#include "semantic.h"
#include "../diagnostic/diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// --- Symbol Table Structures ---

typedef struct SemSymbol {
    char *name;
    VarType type;
    int is_mutable;
    int is_array;
    int array_size; // 0 if unknown or dynamic
    struct SemSymbol *next;
} SemSymbol;

typedef struct SemFunc {
    char *name;
    VarType ret_type;
    struct SemFunc *next;
} SemFunc;

typedef struct SemClass {
    char *name;
    struct SemClass *next;
} SemClass;

typedef struct Scope {
    SemSymbol *symbols;
    struct Scope *parent;
} Scope;

typedef struct {
    Scope *current_scope;
    SemFunc *functions;
    SemClass *classes;
    
    int error_count;
    
    // Context tracking
    VarType current_func_ret_type; // For checking return types
    int in_loop;                   // For checking break/continue
    const char *current_class;     // For 'this' context
    const char *source_code;       // For error reporting
} SemCtx;

// --- Helper Functions ---

static void sem_error(SemCtx *ctx, ASTNode *node, const char *fmt, ...) {
    ctx->error_count++;

    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Create a dummy token for the diagnostic system
    Token t;
    t.line = node ? node->line : 0;
    t.col = node ? node->col : 0;
    t.text = NULL;
    
    Lexer l;
    if (ctx->source_code) {
        lexer_init(&l, ctx->source_code);
    }
    
    report_error(ctx->source_code ? &l : NULL, t, msg);
}

static int are_types_equal(VarType a, VarType b) {
    if (a.base != b.base) {
        // Auto resolves to anything (simplification)
        if (a.base == TYPE_AUTO || b.base == TYPE_AUTO) return 1;
        return 0;
    }
    if (a.ptr_depth != b.ptr_depth) return 0;
    if (a.base == TYPE_CLASS) {
        if (a.class_name && b.class_name) {
            return strcmp(a.class_name, b.class_name) == 0;
        }
        return 0; // Invalid class types
    }
    // Note: Array size/presence is not strictly checked here, handled in context
    return 1;
}

static const char* type_to_str(VarType t) {
    static char buf[64];
    const char *base;
    switch (t.base) {
        case TYPE_INT: base = "int"; break;
        case TYPE_CHAR: base = "char"; break;
        case TYPE_BOOL: base = "bool"; break;
        case TYPE_FLOAT: base = "single"; break;
        case TYPE_DOUBLE: base = "double"; break;
        case TYPE_VOID: base = "void"; break;
        case TYPE_STRING: base = "string"; break;
        case TYPE_CLASS: base = t.class_name ? t.class_name : "class"; break;
        case TYPE_UNKNOWN: base = "unknown"; break;
        case TYPE_AUTO: base = "auto"; break;
        default: base = "???"; break;
    }
    strcpy(buf, base);
    for(int i=0; i<t.ptr_depth; i++) strcat(buf, "*");
    if (t.array_size > 0) {
        char tmp[16]; sprintf(tmp, "[%d]", t.array_size);
        strcat(buf, tmp);
    }
    return buf;
}

// --- Scope Management ---

static void enter_scope(SemCtx *ctx) {
    Scope *s = malloc(sizeof(Scope));
    s->symbols = NULL;
    s->parent = ctx->current_scope;
    ctx->current_scope = s;
}

static void exit_scope(SemCtx *ctx) {
    if (!ctx->current_scope) return;
    Scope *s = ctx->current_scope;
    
    // Free symbols
    SemSymbol *sym = s->symbols;
    while(sym) {
        SemSymbol *next = sym->next;
        free(sym->name);
        free(sym);
        sym = next;
    }
    
    ctx->current_scope = s->parent;
    free(s);
}

static void add_symbol(SemCtx *ctx, const char *name, VarType type, int is_mut, int is_arr, int arr_size) {
    SemSymbol *s = malloc(sizeof(SemSymbol));
    s->name = strdup(name);
    s->type = type;
    s->is_mutable = is_mut;
    s->is_array = is_arr;
    s->array_size = arr_size;
    s->next = ctx->current_scope->symbols;
    ctx->current_scope->symbols = s;
}

static SemSymbol* find_symbol_current_scope(SemCtx *ctx, const char *name) {
    SemSymbol *s = ctx->current_scope->symbols;
    while(s) {
        if (strcmp(s->name, name) == 0) return s;
        s = s->next;
    }
    return NULL;
}

static SemSymbol* find_symbol(SemCtx *ctx, const char *name) {
    Scope *scope = ctx->current_scope;
    while(scope) {
        SemSymbol *s = scope->symbols;
        while(s) {
            if (strcmp(s->name, name) == 0) return s;
            s = s->next;
        }
        scope = scope->parent;
    }
    return NULL;
}

static void add_func(SemCtx *ctx, const char *name, VarType ret) {
    SemFunc *f = malloc(sizeof(SemFunc));
    f->name = strdup(name);
    f->ret_type = ret;
    f->next = ctx->functions;
    ctx->functions = f;
}

static SemFunc* find_func(SemCtx *ctx, const char *name) {
    SemFunc *f = ctx->functions;
    while(f) {
        if (strcmp(f->name, name) == 0) return f;
        f = f->next;
    }
    return NULL;
}

static void add_class(SemCtx *ctx, const char *name) {
    SemClass *c = malloc(sizeof(SemClass));
    c->name = strdup(name);
    c->next = ctx->classes;
    ctx->classes = c;
}

// --- Traversal Prototypes ---

static VarType check_expr(SemCtx *ctx, ASTNode *node);
static void check_stmt(SemCtx *ctx, ASTNode *node);

// --- Checks ---

static VarType check_expr(SemCtx *ctx, ASTNode *node) {
    VarType unknown = {TYPE_UNKNOWN, 0, NULL};
    if (!node) return unknown;

    switch(node->type) {
        case NODE_LITERAL:
            return ((LiteralNode*)node)->var_type;
        
        case NODE_ARRAY_LIT: {
            ArrayLitNode *an = (ArrayLitNode*)node;
            if (!an->elements) {
                // Return unknown array type if empty
                VarType t = {TYPE_UNKNOWN, 0, NULL, 1}; 
                return t;
            }
            VarType first_t = check_expr(ctx, an->elements);
            ASTNode *curr = an->elements->next;
            int count = 1;
            while(curr) {
                VarType t = check_expr(ctx, curr);
                // Allow some flexibility? strict for now
                if (!are_types_equal(first_t, t)) {
                    sem_error(ctx, curr, "Array element type mismatch. Expected '%s', got '%s'", 
                              type_to_str(first_t), type_to_str(t));
                }
                curr = curr->next;
                count++;
            }
            // Propagate the type as an array
            VarType ret = first_t;
            ret.array_size = count; 
            return ret;
        }

        case NODE_VAR_REF: {
            char *name = ((VarRefNode*)node)->name;
            SemSymbol *sym = find_symbol(ctx, name);
            if (!sym) {
                // Check if it's 'this'
                if (strcmp(name, "this") == 0) {
                    if (!ctx->current_class) {
                        sem_error(ctx, node, "'this' used outside of class method");
                        return unknown;
                    }
                    VarType t = {TYPE_CLASS, 1, strdup(ctx->current_class)}; // this is T*
                    return t;
                }
                
                sem_error(ctx, node, "Undefined variable '%s'", name);
                return unknown;
            }
            VarType res = sym->type;
            // If it's an array, ensure type info reflects that
            if (sym->is_array) {
                 res.array_size = sym->array_size > 0 ? sym->array_size : 1; 
            }
            return res;
        }

        case NODE_BINARY_OP: {
            BinaryOpNode *op = (BinaryOpNode*)node;
            VarType l = check_expr(ctx, op->left);
            VarType r = check_expr(ctx, op->right);
            
            if (l.base == TYPE_UNKNOWN || r.base == TYPE_UNKNOWN) return unknown;

            if (!are_types_equal(l, r)) {
                if (!((l.base == TYPE_INT || l.base == TYPE_FLOAT || l.base == TYPE_DOUBLE) && 
                      (r.base == TYPE_INT || r.base == TYPE_FLOAT || r.base == TYPE_DOUBLE))) {
                    sem_error(ctx, node, "Type mismatch in binary operation: '%s' vs '%s'", type_to_str(l), type_to_str(r));
                }
            }
            if (op->op == TOKEN_LT || op->op == TOKEN_GT || op->op == TOKEN_EQ || op->op == TOKEN_NEQ || op->op == TOKEN_LTE || op->op == TOKEN_GTE) {
                VarType bool_t = {TYPE_BOOL, 0, NULL};
                return bool_t;
            }
            return l;
        }

        case NODE_ASSIGN: {
            AssignNode *a = (AssignNode*)node;
            VarType l_type = unknown;
            int is_const = 0;
            
            if (a->name) {
                SemSymbol *sym = find_symbol(ctx, a->name);
                if (!sym) {
                    sem_error(ctx, node, "Assignment to undefined variable '%s'", a->name);
                } else {
                    l_type = sym->type;
                    is_const = !sym->is_mutable;
                }
            } else if (a->target) {
                l_type = check_expr(ctx, a->target);
            }
            
            if (is_const) {
                sem_error(ctx, node, "Cannot assign to immutable variable '%s'", a->name);
            }

            VarType r_type = check_expr(ctx, a->value);
            
            if (l_type.base != TYPE_UNKNOWN && r_type.base != TYPE_UNKNOWN) {
                if (!are_types_equal(l_type, r_type)) {
                    // Implicit cast check (e.g. array -> ptr)
                    int compatible = 0;
                    if (l_type.ptr_depth > 0 && r_type.array_size > 0 && l_type.base == r_type.base) compatible = 1;
                    
                    if (!compatible) {
                         sem_error(ctx, node, "Type mismatch in assignment. Expected '%s', got '%s'", type_to_str(l_type), type_to_str(r_type));
                    }
                }
            }
            return l_type;
        }

        case NODE_CALL: {
            CallNode *c = (CallNode*)node;
            if (strcmp(c->name, "print") == 0 || strcmp(c->name, "printf") == 0) {
                 VarType ret = {TYPE_VOID, 0, NULL};
                 return ret;
            }

            SemFunc *f = find_func(ctx, c->name);
            if (!f) {
                SemClass *cls = ctx->classes;
                int is_cls = 0;
                while(cls) { if(strcmp(cls->name, c->name) == 0) { is_cls=1; break; } cls = cls->next; }
                
                if (is_cls) {
                    VarType ret = {TYPE_CLASS, 0, strdup(c->name)};
                    return ret;
                }

                sem_error(ctx, node, "Undefined function '%s'", c->name);
                return unknown;
            }
            
            ASTNode *arg = c->args;
            while(arg) {
                check_expr(ctx, arg);
                arg = arg->next;
            }
            
            return f->ret_type;
        }
        
        case NODE_ARRAY_ACCESS: {
            ArrayAccessNode *aa = (ArrayAccessNode*)node;
            VarType target_t = check_expr(ctx, aa->target);
            VarType idx_t = check_expr(ctx, aa->index);
            
            if (idx_t.base != TYPE_INT) {
                sem_error(ctx, node, "Array index must be an integer, got '%s'", type_to_str(idx_t));
            }
            
            // Out of Bounds Check (Static)
            if (aa->index->type == NODE_LITERAL) {
                int idx = ((LiteralNode*)aa->index)->val.int_val;
                if (aa->target->type == NODE_VAR_REF) {
                    SemSymbol *sym = find_symbol(ctx, ((VarRefNode*)aa->target)->name);
                    if (sym && sym->is_array && sym->array_size > 0) {
                        if (idx < 0 || idx >= sym->array_size) {
                            sem_error(ctx, node, "Array index %d out of bounds (size %d)", idx, sym->array_size);
                        }
                    }
                }
            }
            
            if (target_t.ptr_depth > 0) target_t.ptr_depth--;
            else if (target_t.array_size > 0) {
                 target_t.array_size = 0; 
            }
            return target_t;
        }
        
        case NODE_MEMBER_ACCESS: {
             MemberAccessNode *ma = (MemberAccessNode*)node;
             VarType t = check_expr(ctx, ma->object);
             // Cannot easily verify member existence/type without tracking struct definitions fully.
             // For now, assume it returns Unknown to avoid blocking compilation, or try best effort if class info available.
             // TODO: Enhance class member tracking
             VarType ret = {TYPE_UNKNOWN, 0, NULL};
             return ret;
        }

        default:
            return unknown;
    }
}

static void check_stmt(SemCtx *ctx, ASTNode *node) {
    if (!node) return;
    
    switch(node->type) {
        case NODE_VAR_DECL: {
            VarDeclNode *vd = (VarDeclNode*)node;
            if (find_symbol_current_scope(ctx, vd->name)) {
                sem_error(ctx, node, "Redefinition of variable '%s' in current scope", vd->name);
            }
            
            VarType inferred = vd->var_type;
            if (vd->var_type.base == TYPE_AUTO) {
                if (!vd->initializer) {
                    sem_error(ctx, node, "Cannot infer type for '%s' without initializer", vd->name);
                    inferred.base = TYPE_INT; 
                } else {
                    inferred = check_expr(ctx, vd->initializer);
                }
            } else if (vd->initializer) {
                VarType init_t = check_expr(ctx, vd->initializer);
                if (!are_types_equal(vd->var_type, init_t)) {
                     int ok = 0;
                     if (vd->var_type.base == TYPE_STRING && init_t.base == TYPE_STRING) ok = 1;
                     // String literal to char array
                     if (vd->var_type.base == TYPE_CHAR && vd->is_array && init_t.base == TYPE_STRING) ok = 1;
                     
                     if (!ok) {
                        sem_error(ctx, node, "Variable '%s' type mismatch. Declared '%s', init '%s'", 
                                  vd->name, type_to_str(vd->var_type), type_to_str(init_t));
                     }
                } else {
                    // Types match base, check array compatibility
                    if (vd->is_array) {
                        if (init_t.array_size <= 0 && init_t.ptr_depth == 0) {
                             // Trying to assign scalar to array
                             sem_error(ctx, node, "Cannot initialize array '%s' with scalar type '%s'", 
                                      vd->name, type_to_str(init_t));
                        }
                    } else {
                        if (init_t.array_size > 0) {
                             // Trying to assign array to scalar
                             sem_error(ctx, node, "Cannot initialize scalar '%s' with array type '%s'", 
                                      vd->name, type_to_str(init_t));
                        }
                    }
                }
            }
            
            int arr_size = 0;
            if (vd->is_array) {
                if (vd->array_size && vd->array_size->type == NODE_LITERAL) {
                    arr_size = ((LiteralNode*)vd->array_size)->val.int_val;
                } else if (vd->initializer && vd->initializer->type == NODE_LITERAL && ((LiteralNode*)vd->initializer)->var_type.base == TYPE_STRING) {
                     arr_size = strlen(((LiteralNode*)vd->initializer)->val.str_val) + 1;
                } else if (vd->initializer && vd->initializer->type == NODE_ARRAY_LIT) {
                     // Get size from array literal
                     ASTNode* el = ((ArrayLitNode*)vd->initializer)->elements;
                     while(el) { arr_size++; el = el->next; }
                }
            }

            add_symbol(ctx, vd->name, inferred, vd->is_mutable, vd->is_array, arr_size);
            break;
        }

        case NODE_RETURN: {
            ReturnNode *r = (ReturnNode*)node;
            VarType ret_t = {TYPE_VOID, 0, NULL};
            if (r->value) ret_t = check_expr(ctx, r->value);
            
            if (!are_types_equal(ctx->current_func_ret_type, ret_t)) {
                sem_error(ctx, node, "Return type mismatch. Expected '%s', got '%s'", 
                          type_to_str(ctx->current_func_ret_type), type_to_str(ret_t));
            }
            break;
        }

        case NODE_IF: {
            IfNode *i = (IfNode*)node;
            check_expr(ctx, i->condition);
            enter_scope(ctx);
            check_stmt(ctx, i->then_body);
            exit_scope(ctx);
            if (i->else_body) {
                enter_scope(ctx);
                check_stmt(ctx, i->else_body);
                exit_scope(ctx);
            }
            break;
        }

        case NODE_LOOP: {
            LoopNode *l = (LoopNode*)node;
            check_expr(ctx, l->iterations);
            int prev_loop = ctx->in_loop;
            ctx->in_loop = 1;
            enter_scope(ctx);
            check_stmt(ctx, l->body);
            exit_scope(ctx);
            ctx->in_loop = prev_loop;
            break;
        }
        
        case NODE_WHILE: {
            WhileNode *w = (WhileNode*)node;
            check_expr(ctx, w->condition);
            int prev_loop = ctx->in_loop;
            ctx->in_loop = 1;
            enter_scope(ctx);
            check_stmt(ctx, w->body);
            exit_scope(ctx);
            ctx->in_loop = prev_loop;
            break;
        }

        case NODE_BREAK:
        case NODE_CONTINUE:
            if (!ctx->in_loop) {
                sem_error(ctx, node, "'break' or 'continue' used outside of loop");
            }
            break;

        case NODE_FUNC_DEF:
            break;

        default:
            check_expr(ctx, node); 
            break;
    }
    
    if (node->next) check_stmt(ctx, node->next);
}

// --- Driver Logic ---

static void scan_declarations(SemCtx *ctx, ASTNode *node) {
    while(node) {
        if (node->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)node;
            add_func(ctx, fd->name, fd->ret_type);
        } 
        else if (node->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)node;
            add_class(ctx, cn->name);
        }
        else if (node->type == NODE_NAMESPACE) {
             scan_declarations(ctx, ((NamespaceNode*)node)->body);
        }
        node = node->next;
    }
}

static void check_program(SemCtx *ctx, ASTNode *node) {
    while(node) {
        if (node->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)node;
            ctx->current_func_ret_type = fd->ret_type;
            enter_scope(ctx);
            
            Parameter *p = fd->params;
            while(p) {
                add_symbol(ctx, p->name, p->type, 1, 0, 0); 
                p = p->next;
            }
            
            if (fd->class_name) {
                 ctx->current_class = fd->class_name;
            }

            check_stmt(ctx, fd->body);
            
            ctx->current_class = NULL;
            exit_scope(ctx);
        }
        else if (node->type == NODE_VAR_DECL) {
             check_stmt(ctx, node); 
        }
        else if (node->type == NODE_CLASS) {
             // Basic class check
        }
        else {
            check_stmt(ctx, node);
        }
        node = node->next;
    }
}

int semantic_analysis(ASTNode *root, const char *source) {
    SemCtx ctx;
    ctx.current_scope = NULL;
    ctx.functions = NULL;
    ctx.classes = NULL;
    ctx.error_count = 0;
    ctx.in_loop = 0;
    ctx.current_class = NULL;
    ctx.source_code = source;
    
    enter_scope(&ctx);
    
    // Pass 1: Register top-level symbols
    scan_declarations(&ctx, root);
    
    // Pass 2: Verify bodies
    check_program(&ctx, root);
    
    exit_scope(&ctx);
    
    while(ctx.functions) { SemFunc *n = ctx.functions->next; free(ctx.functions->name); free(ctx.functions); ctx.functions = n; }
    while(ctx.classes) { SemClass *n = ctx.classes->next; free(ctx.classes->name); free(ctx.classes); ctx.classes = n; }

    return ctx.error_count;
}
