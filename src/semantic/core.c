#include "semantic.h"

void sem_hint(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (ctx->current_source && node) {
        Lexer l;
        lexer_init(&l, ctx->current_source);
        l.filename = (char*)ctx->current_filename;
        
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
