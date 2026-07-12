#include "meta/eval.h"
#include <stdio.h>
#include <stdlib.h>

void meta_init(CompilerContext *ctx) {
    // Setup meta environment (symbol tables for interpreter)
}

MetaValue meta_eval_expr(ASTNode *expr) {
    MetaValue val = { .type = VAL_NULL, .as.int_val = 0 };
    if (!expr) return val;
    
    switch (expr->type) {
        case NODE_LITERAL: {
            LiteralNode *lit = (LiteralNode*)expr;
            if (lit->var_type.base == TYPE_INT) {
                val.type = VAL_INT;
                val.as.int_val = lit->val.int_val;
            } else if (lit->var_type.base == TYPE_STRING) {
                val.type = VAL_STRING;
                val.as.str_val = lit->val.str_val;
            }
            break;
        }
        case NODE_BINARY_OP: {
            BinaryOpNode *bin = (BinaryOpNode*)expr;
            MetaValue left = meta_eval_expr(bin->left);
            MetaValue right = meta_eval_expr(bin->right);
            
            if (left.type == VAL_INT && right.type == VAL_INT) {
                val.type = VAL_INT;
                if (bin->op == TOKEN_PLUS) val.as.int_val = left.as.int_val + right.as.int_val;
                else if (bin->op == TOKEN_MINUS) val.as.int_val = left.as.int_val - right.as.int_val;
                else if (bin->op == TOKEN_LT) { val.type = VAL_BOOL; val.as.bool_val = left.as.int_val < right.as.int_val; }
                else if (bin->op == TOKEN_GT) { val.type = VAL_BOOL; val.as.bool_val = left.as.int_val > right.as.int_val; }
            }
            break;
        }
        default:
            break; // Unhandled yet
    }
    return val;
}

void meta_eval_stmt(ASTNode *stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case NODE_IF: {
            IfNode *ifn = (IfNode*)stmt;
            MetaValue cond = meta_eval_expr(ifn->condition);
            bool is_true = false;
            
            if (cond.type == VAL_BOOL) is_true = cond.as.bool_val;
            else if (cond.type == VAL_INT) is_true = (cond.as.int_val != 0);
            
            if (is_true) {
                ASTNode *curr = ifn->then_body;
                while (curr) { meta_eval_stmt(curr); curr = curr->next; }
            } else if (ifn->else_body) {
                ASTNode *curr = ifn->else_body;
                while (curr) { meta_eval_stmt(curr); curr = curr->next; }
            }
            break;
        }
        case NODE_WHILE: {
            WhileNode *whn = (WhileNode*)stmt;
            while (1) {
                MetaValue cond = meta_eval_expr(whn->condition);
                bool is_true = false;
                if (cond.type == VAL_BOOL) is_true = cond.as.bool_val;
                else if (cond.type == VAL_INT) is_true = (cond.as.int_val != 0);
                
                if (!is_true) break;
                
                ASTNode *curr = whn->body;
                while (curr) { meta_eval_stmt(curr); curr = curr->next; }
            }
            break;
        }
        case NODE_CALL: {
            // Built-in functions like print()
            MethodCallNode *call = (MethodCallNode*)stmt;
            if (call->name && strcmp(call->name, "print") == 0) {
                ASTNode *arg = call->args;
                while (arg) {
                    MetaValue v = meta_eval_expr(arg);
                    if (v.type == VAL_INT) printf("%lld", v.as.int_val);
                    else if (v.type == VAL_STRING) printf("%s", v.as.str_val);
                    arg = arg->next;
                }
                printf("\n");
            }
            break;
        }
        default:
            break; // Unhandled yet
    }
}
