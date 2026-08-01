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
            
            void* if_op = alkyl_mlir_build_scf_if_start(ctx, cond, if_node->else_body != NULL);
            ASTNode *stmt = if_node->then_body;
            while(stmt) { mlir_gen_stmt(ctx, mod, stmt); stmt = stmt->next; }
            
            if (if_node->else_body) {
                alkyl_mlir_build_scf_if_else(ctx, if_op);
                stmt = if_node->else_body;
                while(stmt) { mlir_gen_stmt(ctx, mod, stmt); stmt = stmt->next; }
            }
            alkyl_mlir_build_scf_if_end(ctx, if_op);
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
        case NODE_SWITCH: {
            SwitchNode *sw = (SwitchNode*)node;
            AlkylMlirValue cond = mlir_gen_expr(ctx, mod, sw->condition);
            
            // Count cases
            int num_cases = 0;
            ASTNode *curr = sw->cases;
            while (curr) { if (curr->type == NODE_CASE) num_cases++; curr = curr->next; }
            
            void* switch_op = alkyl_mlir_build_switch_start(ctx, cond, num_cases);
            
            curr = sw->cases;
            while (curr) {
                if (curr->type == NODE_CASE) {
                    CaseNode *c = (CaseNode*)curr;
                    AlkylMlirValue case_val = NULL;
                    if (c->value) {
                        case_val = mlir_gen_expr(ctx, mod, c->value);
                    }
                    alkyl_mlir_build_switch_case_start(ctx, switch_op, case_val, c->is_leak);
                    
                    ASTNode *body_stmt = c->body;
                    while (body_stmt) {
                        mlir_gen_stmt(ctx, mod, body_stmt);
                        body_stmt = body_stmt->next;
                    }
                    alkyl_mlir_build_switch_case_end(ctx, switch_op);
                }
                curr = curr->next;
            }
            
            if (sw->default_case) {
                alkyl_mlir_build_switch_default_start(ctx, switch_op);
                ASTNode *def_stmt = sw->default_case;
                while (def_stmt) {
                    mlir_gen_stmt(ctx, mod, def_stmt);
                    def_stmt = def_stmt->next;
                }
                alkyl_mlir_build_switch_default_end(ctx, switch_op);
            }
            
            alkyl_mlir_build_switch_end(ctx, switch_op);
            break;
        }
        default:
            printf("Warning: Unhandled stmt node type %d in MLIR\n", node->type);
            break;
    }
}
