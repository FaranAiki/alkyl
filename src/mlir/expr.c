#include "mlir/generate.h"
#include <stdio.h>

AlkylMlirValue mlir_gen_expr(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case NODE_LITERAL: {
            LiteralNode *lit = (LiteralNode*)node;
            // For mock purposes, assume it's an int literal
            return alkyl_mlir_build_int_constant(ctx, lit->val.int_val);
            // Add floats, strings, etc. here
            break;
        }
        case NODE_BINARY_OP: {
            BinaryOpNode *binop = (BinaryOpNode*)node;
            AlkylMlirValue lhs = mlir_gen_expr(ctx, mod, binop->left);
            AlkylMlirValue rhs = mlir_gen_expr(ctx, mod, binop->right);
            
            if (binop->op == TOKEN_PLUS) return alkyl_mlir_build_add(ctx, lhs, rhs);
            if (binop->op == TOKEN_MINUS) return alkyl_mlir_build_sub(ctx, lhs, rhs);
            if (binop->op == TOKEN_STAR) return alkyl_mlir_build_mul(ctx, lhs, rhs);
            if (binop->op == TOKEN_SLASH) return alkyl_mlir_build_div(ctx, lhs, rhs);
            if (binop->op == TOKEN_LSHIFT) return alkyl_mlir_build_shl(ctx, lhs, rhs);
            if (binop->op == TOKEN_RSHIFT) return alkyl_mlir_build_shr(ctx, lhs, rhs);
            break;
        }
        default:
            printf("Warning: Unhandled expr node type %d in MLIR\n", node->type);
            break;
    }
    return NULL;
}
