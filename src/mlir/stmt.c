#include "mlir/generate.h"
#include <stdio.h>

void mlir_gen_stmt(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_RETURN: {
            ReturnNode *ret = (ReturnNode*)node;
            AlkylMlirValue val = NULL;
            if (ret->value) {
                val = mlir_gen_expr(ctx, mod, ret->value);
            }
            alkyl_mlir_build_return(ctx, val);
            break;
        }
        case NODE_VAR_DECL: {
            VarDeclNode *var = (VarDeclNode*)node;
            AlkylMlirValue alloca = alkyl_mlir_build_alloca(ctx, var->name);
            
            if (var->initializer) {
                AlkylMlirValue init_val = mlir_gen_expr(ctx, mod, var->initializer);
                alkyl_mlir_build_store(ctx, init_val, alloca);
            }
            break;
        }
        case NODE_IF: {
            IfNode *if_node = (IfNode*)node;
            AlkylMlirValue cond = mlir_gen_expr(ctx, mod, if_node->condition);
            alkyl_mlir_build_scf_if(ctx, cond);
            // In a full implementation, we would generate then_body and else_body inside the SCF regions.
            break;
        }
        case NODE_WHILE: {
            WhileNode *while_node = (WhileNode*)node;
            AlkylMlirValue cond = mlir_gen_expr(ctx, mod, while_node->condition);
            alkyl_mlir_build_scf_while(ctx, cond);
            break;
        }
        case NODE_FOR_IN: {
            // Placeholder: For-In loops translate to SCF For or While loops
            break;
        }
        default:
            printf("Warning: Unhandled stmt node type %d in MLIR\n", node->type);
            break;
    }
}
