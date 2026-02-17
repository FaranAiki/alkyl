#include "semantic.h"

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
