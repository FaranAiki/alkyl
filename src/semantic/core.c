#include "semantic.h"
#include <stdarg.h>
#include <stdio.h>

// Helper to construct a temporary lexer for reporting
static void setup_report_lexer(Lexer *l, SemanticCtx *ctx) {
    if (ctx->compiler_ctx) {
        // Initialize with the context and source
        // We assume lexer_init handles basic setup
        lexer_init(l, ctx->compiler_ctx, ctx->current_filename, ctx->current_source, NULL);
    } else {
        // Fallback if no compiler context (shouldn't happen in proper flow)
        l->ctx = NULL;
        l->src = ctx->current_source;
        l->filename = (char*)ctx->current_filename;
    }
}

void sem_hint(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (ctx->current_source && node) {
        Lexer l;
        setup_report_lexer(&l, ctx);
        
        Token t;
        t.line = node->line;
        t.col = node->col;
        t.type = TOKEN_UNKNOWN; 
        t.text = NULL;
        
        report_hint(&l, t, msg);
    } else {
        fprintf(stderr, "[Semantic Hint] %s\n", msg);
    }
}

void sem_error(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...) {
    if (ctx->compiler_ctx) {
        ctx->compiler_ctx->semantic_error_count++;
    }
    
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (ctx->current_source && node) {
        Lexer l;
        setup_report_lexer(&l, ctx);
        
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
// DFS to check for size cycles
static int check_class_size_cycle(SemanticCtx *ctx, SemSymbol *sym) {
    if (!sym || sym->kind != SYM_CLASS) return 1;
    if (sym->must_pure) { // visiting
        sem_error(ctx, NULL, "Cyclic dependency detected in class '%s' size", sym->name);
        return 0; // Cycle detected
    }
    if (sym->must_pristine) return 1; // already visited
    
    sym->must_pure = 1; // mark as visiting
    
    // Check parent
    if (sym->parent_name) {
        SemSymbol *p = sem_symbol_lookup(ctx, sym->parent_name, NULL);
        if (p && !check_class_size_cycle(ctx, p)) return 0;
    }
    // Check traits
    for (int i = 0; i < sym->trait_count; i++) {
        SemSymbol *t = sem_symbol_lookup(ctx, sym->traits[i], NULL);
        if (t && !check_class_size_cycle(ctx, t)) return 0;
    }
    // Check fields
    if (sym->inner_scope) {
        SemSymbol *f = sym->inner_scope->symbols;
        while (f) {
            if (f->kind == SYM_VAR && f->type.base == TYPE_CLASS && f->type.ptr_depth == 0) {
                SemSymbol *fsym = sem_symbol_lookup(ctx, f->type.class_name, NULL);
                if (fsym && !check_class_size_cycle(ctx, fsym)) return 0;
            }
            f = f->next;
        }
    }
    
    sym->must_pure = 0;
    sym->must_pristine = 1; // marked as fully visited
    return 1;
}

void sem_warning(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (ctx->current_source && node) {
        Lexer l;
        setup_report_lexer(&l, ctx);
        
        Token t;
        t.line = node->line;
        t.col = node->col;
        t.type = TOKEN_UNKNOWN; 
        t.text = NULL;
        
        report_warning(&l, t, msg);
    } else {
        if (node) {
            fprintf(stderr, "[Semantic Warning] Line %d, Col %d: %s\n", node->line, node->col, msg);
        } else {
            fprintf(stderr, "[Semantic Warning] %s\n", msg);
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
        setup_report_lexer(&l, ctx);
        
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

void sem_register_builtins(SemanticCtx *ctx) {
    if (ctx->compiler_ctx) {
        VarType err_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
        
        // Always inject ErrNull to the global scope
        SemSymbol *sym_null = sem_symbol_add(ctx, "ErrNull", SYM_VAR, err_type);
        sym_null->is_initialized = 1;
        sym_null->is_mutable = 0;
        
        if (!ctx->compiler_ctx->settings.no_purge) {
            if (!hashmap_get(&ctx->compiler_ctx->error_table, "ErrDivisionByZero")) {
                int id = ctx->compiler_ctx->next_error_id++;
                hashmap_put(&ctx->compiler_ctx->error_table, "ErrDivisionByZero", (void*)(intptr_t)(id + 1));
            }
            SemSymbol *sym_div = sem_symbol_add(ctx, "ErrDivisionByZero", SYM_VAR, err_type);
            sym_div->is_initialized = 1;
            sym_div->is_mutable = 0;
        }
    }
}

int sem_check_program(SemanticCtx *ctx, ASTNode *root) {
    if (!root) return 0;
    
    ASTNode **tail = &root;
    while (*tail && (*tail)->next) {
        tail = &(*tail)->next;
    }
    if (*tail) {
        ctx->ast_tail = &(*tail)->next;
    } else {
        ctx->ast_tail = tail;
    }
    
    sem_register_builtins(ctx);
    sem_scan_top_level(ctx, root);
    
    int current_errors = 0;
    if (ctx->compiler_ctx) current_errors = ctx->compiler_ctx->semantic_error_count;

    if (current_errors > 0) return current_errors;
    
    // Pass 1.5: Structural validations (Inheritance, Traits)
    ASTNode *curr_val = root;
    while (curr_val) {
        if (curr_val->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)curr_val;
            if (cn->parent_name) {
                SemSymbol *parent = sem_symbol_lookup(ctx, cn->parent_name, NULL);
                if (parent && parent->kind == SYM_CLASS) {
                    if (parent->is_is_a == IS_A_FINAL) {
                        sem_error(ctx, curr_val, "Class '%s' cannot inherit from final class '%s'", cn->name, cn->parent_name);
                    }
                    parent->is_used_as_parent = 1;
                }
            }
            for (int i = 0; i < cn->traits.count; i++) {
                SemSymbol *trait = sem_symbol_lookup(ctx, cn->traits.names[i], NULL);
                if (trait && trait->kind == SYM_CLASS) {
                    if (trait->is_has_a == HAS_A_INERT) {
                        sem_error(ctx, curr_val, "Class '%s' is inert, thus cannot be explicitly composed in '%s'", trait->name, cn->name);
                    }
                    trait->is_used_as_composition = 1;
                }
            }
        }
        curr_val = curr_val->next;
    }

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
    
    // Cycle Detection for Class Sizes
    if (ctx->global_scope) {
        SemSymbol *gsym = ctx->global_scope->symbols;
        while (gsym) {
            if (gsym->kind == SYM_CLASS) {
                // Clear visit state
                SemSymbol *r = ctx->global_scope->symbols;
                while(r) { r->must_pure = 0; r->must_pristine = 0; r = r->next; }
                if (!check_class_size_cycle(ctx, gsym)) {
                    // Stop checking further classes if error is hit
                    break;
                }
            }
            gsym = gsym->next;
        }
    }
    
    // Final structural validations (naked, reactive) globally
    if (ctx->global_scope) {
        SemSymbol *gsym = ctx->global_scope->symbols;
        while (gsym) {
            if (gsym->kind == SYM_CLASS) {
                if (gsym->is_is_a == IS_A_NAKED && !gsym->is_used_as_parent) {
                    sem_error(ctx, NULL, "Class '%s' is marked naked but is never inherited", gsym->name);
                }
                if (gsym->is_has_a == HAS_A_REACTIVE && !gsym->is_used_as_composition) {
                    sem_error(ctx, NULL, "Class '%s' is marked reactive but is never composed (has-ed)", gsym->name);
                }
            }
            gsym = gsym->next;
        }
    }

    if (ctx->compiler_ctx) return ctx->compiler_ctx->error_count;
    return 0;
}
