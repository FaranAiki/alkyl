/**
 * @file stmt.c
 * @brief MLIR statement generation implementation.
 */
#include "mlir/generate.h"
#include "common/hashmap.h"
#include <stdio.h>
#include <stdlib.h>

ASTNode **mlir_defers = NULL;
int mlir_defer_count = 0;
int mlir_defer_capacity = 0;

void reset_mlir_defers() {
    mlir_defer_count = 0;
}

void execute_mlir_defers(AlkylMlirContext ctx, AlkylMlirModule mod) {
    for (int i = mlir_defer_count - 1; i >= 0; i--) {
        mlir_gen_stmt(ctx, mod, mlir_defers[i]);
    }
}

void mlir_gen_stmt(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_DEFER: {
            DeferNode *defer = (DeferNode*)node;
            if (mlir_defer_count >= mlir_defer_capacity) {
                mlir_defer_capacity = mlir_defer_capacity == 0 ? 8 : mlir_defer_capacity * 2;
                mlir_defers = realloc(mlir_defers, sizeof(ASTNode*) * mlir_defer_capacity);
            }
            mlir_defers[mlir_defer_count++] = defer->body;
            break;
        }
        case NODE_RETURN: {
            execute_mlir_defers(ctx, mod);
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
            
            extern HashMap *mlir_vars;
            if (mlir_vars && var->name) {
                hashmap_put(mlir_vars, var->name, alloca);
            }
            
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
        case NODE_BREAK:
            alkyl_mlir_build_scf_break(ctx);
            break;
        case NODE_CONTINUE:
            alkyl_mlir_build_scf_continue(ctx);
            break;
        case NODE_WHILE: {
            WhileNode *while_node = (WhileNode*)node;
            void *while_op = alkyl_mlir_build_scf_while_start(ctx);
            AlkylMlirValue cond = mlir_gen_expr(ctx, mod, while_node->condition);
            alkyl_mlir_build_scf_while_cond_yield(ctx, while_op, cond);
            
            ASTNode *stmt = while_node->body;
            while(stmt) { mlir_gen_stmt(ctx, mod, stmt); stmt = stmt->next; }
            alkyl_mlir_build_scf_while_end(ctx, while_op);
            break;
        }
        case NODE_PURGE: {
            PurgeNode *pn = (PurgeNode*)node;
            VarRefNode *vr = (VarRefNode*)pn->msg;
            int err_id = 1;
            alkyl_mlir_build_panic(ctx, err_id, vr->name);
            break;
        }
        case NODE_CLEAN: {
            CleanNode *cn = (CleanNode*)node;
            extern HashMap *mlir_vars;
            AlkylMlirValue target_ptr = hashmap_get(mlir_vars, cn->var_name);
            AlkylMlirValue target_val = alkyl_mlir_build_load(ctx, target_ptr);
            
            AlkylMlirValue err_code = alkyl_mlir_build_load_field(ctx, target_val, 0, 0);
            AlkylMlirValue zero = alkyl_mlir_build_int_constant(ctx, 0);
            AlkylMlirValue cond = alkyl_mlir_build_eq(ctx, err_code, zero);
            
            void* if_op = alkyl_mlir_build_scf_if_start(ctx, cond, cn->residue_body != NULL || cn->residue_cases != NULL);
            
            AlkylMlirValue pristine_val = alkyl_mlir_build_load_field(ctx, target_val, 1, 0);
            const char *p_name = cn->pristine_var_name ? cn->pristine_var_name : cn->var_name;
            AlkylMlirValue p_alloc = alkyl_mlir_build_alloca(ctx, p_name);
            hashmap_put(mlir_vars, p_name, p_alloc);
            alkyl_mlir_build_store(ctx, pristine_val, p_alloc);
            
            ASTNode *stmt = cn->body;
            while(stmt) { mlir_gen_stmt(ctx, mod, stmt); stmt = stmt->next; }
            
            if (cn->residue_body || cn->residue_cases) {
                alkyl_mlir_build_scf_if_else(ctx, if_op);
                if (cn->err_var_name) {
                    AlkylMlirValue e_alloc = alkyl_mlir_build_alloca(ctx, cn->err_var_name);
                    hashmap_put(mlir_vars, cn->err_var_name, e_alloc);
                    alkyl_mlir_build_store(ctx, err_code, e_alloc);
                }
                
                ResidueCase *rc = cn->residue_cases;
                while (rc) {
                    ASTNode *r_stmt = rc->body;
                    while(r_stmt) { mlir_gen_stmt(ctx, mod, r_stmt); r_stmt = r_stmt->next; }
                    rc = rc->next;
                }
                stmt = cn->residue_body;
                while(stmt) { mlir_gen_stmt(ctx, mod, stmt); stmt = stmt->next; }
            }
            alkyl_mlir_build_scf_if_end(ctx, if_op);
            break;
        }
        case NODE_UNTAINT: {
            UntaintNode *un = (UntaintNode*)node;
            extern HashMap *mlir_vars;
            AlkylMlirValue target_ptr = hashmap_get(mlir_vars, un->var_name);
            AlkylMlirValue target_val = alkyl_mlir_build_load(ctx, target_ptr);
            
            AlkylMlirValue pristine_val = alkyl_mlir_build_load_field(ctx, target_val, 1, 0);
            AlkylMlirValue p_alloc = alkyl_mlir_build_alloca(ctx, un->var_name);
            hashmap_put(mlir_vars, un->var_name, p_alloc);
            alkyl_mlir_build_store(ctx, pristine_val, p_alloc);
            break;
        }
        case NODE_WASH: {
            break;
        }
        case NODE_FOR_IN: {
            ForInNode *for_in = (ForInNode*)node;
            void *while_op = alkyl_mlir_build_scf_while_start(ctx);
            
            // For now, use collection as the dummy condition in the while loop
            // In a real iterator, we'd invoke collection.__has_next__() here
            AlkylMlirValue collection_cond = mlir_gen_expr(ctx, mod, for_in->collection);
            alkyl_mlir_build_scf_while_cond_yield(ctx, while_op, collection_cond);
            
            extern HashMap *mlir_vars;
            if (mlir_vars && for_in->var_name) {
                AlkylMlirValue iter_alloca = alkyl_mlir_build_alloca(ctx, for_in->var_name);
                hashmap_put(mlir_vars, for_in->var_name, iter_alloca);
            }
            
            ASTNode *stmt = for_in->body;
            while(stmt) { mlir_gen_stmt(ctx, mod, stmt); stmt = stmt->next; }
            alkyl_mlir_build_scf_while_end(ctx, while_op);
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
                        void alkyl_mlir_build_switch_set_cond_insertion(AlkylMlirContext c_ctx, void* switch_op_ptr);
                        alkyl_mlir_build_switch_set_cond_insertion(ctx, switch_op);
                        case_val = mlir_gen_expr(ctx, mod, c->value);
                    }
                    alkyl_mlir_build_switch_case_start(ctx, switch_op, case_val, c->is_leak);
                    
                    ASTNode *body_stmt = c->body;
                    while (body_stmt) {
                        mlir_gen_stmt(ctx, mod, body_stmt);
                        body_stmt = body_stmt->next;
                    }
                    void alkyl_mlir_build_switch_case_end(AlkylMlirContext c_ctx, void* switch_op_ptr, int is_leak);
                    alkyl_mlir_build_switch_case_end(ctx, switch_op, c->is_leak);
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
        case NODE_ASSIGN:
        case NODE_INC_DEC:
        case NODE_METHOD_CALL:
        case NODE_MEMBER_ACCESS:
        case NODE_CALL:
        case NODE_VAR_REF:
            // Delegate statement-level expression evaluation to expr.c
            mlir_gen_expr(ctx, mod, node);
            break;
        default:
            printf("Warning: Unhandled stmt node type %d in MLIR\n", node->type);
            break;
    }
}
