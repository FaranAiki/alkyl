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
        case NODE_VAR_REF: {
            VarRefNode *v = (VarRefNode*)node;
            // For now, in a full implementation we'd look up the alloca from a scope map.
            // As a stub, we will just return a dummy load or constant 0.
            return alkyl_mlir_build_int_constant(ctx, 0); 
        }
        case NODE_CALL: {
            CallNode *call = (CallNode*)node;
            // Evaluate arguments
            AlkylMlirValue args[32];
            int num_args = 0;
            ASTNode *arg = call->args;
            while (arg && num_args < 32) {
                args[num_args++] = mlir_gen_expr(ctx, mod, arg);
                arg = arg->next;
            }
            return alkyl_mlir_build_call(ctx, call->name ? call->name : "unknown", args, num_args);
        }
        case NODE_BINARY_OP: {
            BinaryOpNode *binop = (BinaryOpNode*)node;
            AlkylMlirValue lhs = mlir_gen_expr(ctx, mod, binop->left);
            AlkylMlirValue rhs = mlir_gen_expr(ctx, mod, binop->right);
            
            if (binop->op == TOKEN_PLUS) return alkyl_mlir_build_add(ctx, lhs, rhs);
            if (binop->op == TOKEN_MINUS) return alkyl_mlir_build_sub(ctx, lhs, rhs);
            if (binop->op == TOKEN_STAR) return alkyl_mlir_build_mul(ctx, lhs, rhs);
            if (binop->op == TOKEN_SLASH) return alkyl_mlir_build_div(ctx, lhs, rhs);
            if (binop->op == TOKEN_MOD) return alkyl_mlir_build_mod(ctx, lhs, rhs);
            if (binop->op == TOKEN_LSHIFT) return alkyl_mlir_build_shl(ctx, lhs, rhs);
            if (binop->op == TOKEN_RSHIFT) return alkyl_mlir_build_shr(ctx, lhs, rhs);
            if (binop->op == TOKEN_AND) return alkyl_mlir_build_and(ctx, lhs, rhs);
            if (binop->op == TOKEN_OR) return alkyl_mlir_build_or(ctx, lhs, rhs);
            if (binop->op == TOKEN_XOR) return alkyl_mlir_build_xor(ctx, lhs, rhs);
            break;
        }
        default:
            printf("Warning: Unhandled expr node type %d in MLIR\n", node->type);
            break;
    }
    return NULL;
}
