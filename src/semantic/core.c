#include "semantic.h"
#include <stdarg.h>
#include <stdio.h>

// Helper to construct a temporary lexer for reporting
static void setup_report_lexer(Lexer *l, SemanticCtx *ctx) {
    if (ctx->compiler_ctx) {
        // Initialize with the context and source
        // We assume lexer_init handles basic setup
        lexer_init(l, ctx->compiler_ctx, ctx->current_filename, ctx->current_source);
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
        ctx->compiler_ctx->error_count++;
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

int sem_check_program(SemanticCtx *ctx, ASTNode *root) {
    if (!root) return 0;
    
    sem_register_builtins(ctx);
    sem_scan_top_level(ctx, root);
    
    int current_errors = 0;
    if (ctx->compiler_ctx) current_errors = ctx->compiler_ctx->semantic_error_count;

    if (current_errors > 0) return current_errors;
    
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
    
    if (ctx->compiler_ctx) return ctx->compiler_ctx->semantic_error_count;
    return 0;
}
