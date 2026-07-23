#include "alir.h"
#include "semantic/semantic.h"
#include "meta/vm.h"

void alir_gen_stmt(AlirCtx *ctx, ASTNode *node) {
    if (!node) return;
    if (ctx->current_block && ctx->current_block->tail && is_terminator(ctx->current_block->tail->op)) return;

    ctx->current_line = node->line;
    ctx->current_col = node->col;

    if (node->type == NODE_VAR_DECL && ctx->in_flux_resume) {
        VarDeclNode *vn = (VarDeclNode*)node;
        FluxVar *fv = ctx->flux_vars;
        while(fv) {
            if (strcmp(fv->name, vn->name) == 0) break;
            fv = fv->next;
        }

        if (fv) {
            AlirSymbol *sym = alir_find_symbol(ctx, vn->name);
            if (sym && vn->initializer) {
                AlirValue *val = alir_gen_expr(ctx, vn->initializer);
                if (!val) {
                    val = alir_const_int(ctx->module, 0); // Safety net
                }
                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, val, sym->ptr));
            }
            return;
        }
    }

    switch(node->type) {
        // in case a pristine function STILL contains error, then purge here
        // purge SHOULD NOT be in alir: it is type check/error
        case NODE_PURGE: {
            PurgeNode *pn = (PurgeNode*)node;
            VarRefNode *vr = (VarRefNode*)pn->msg;
            int err_id = 1;
            if (ctx->sem && ctx->sem->compiler_ctx) {
                void *val = hashmap_get(&ctx->sem->compiler_ctx->error_table, vr->name);
                if (val) err_id = (int)(intptr_t)val;
            }
            char buf[512];
            // TODO fix this
            snprintf(buf, sizeof(buf), "purge: %s\n", vr->name);
            VarType str_type = { .base = TYPE_CLASS, .class_name = (char*)"string", .ptr_depth = 0 };
            AlirValue *msg_val = alir_module_add_string_literal(ctx->module, buf, str_type, ctx->str_counter++);
            AlirValue *id_val = alir_const_int(ctx->module, err_id);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_PANIC, NULL, msg_val, id_val));
            break;
        }
        case NODE_CLEAN: {
            CleanNode *cn = (CleanNode*)node;
            AlirSymbol *target_sym = alir_find_symbol(ctx, cn->var_name);
            
            VarType pristine_type = target_sym->type;
            pristine_type.is_tainted = 0;
            AlirValue *pristine_ptr = new_temp(ctx, pristine_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, pristine_ptr, NULL, NULL));
            const char *target_name = cn->pristine_var_name ? cn->pristine_var_name : cn->var_name;
            alir_add_symbol(ctx, target_name, pristine_ptr, pristine_type);
            
            AlirValue *err_code_ptr = new_temp(ctx, (VarType){TYPE_INT, 1, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, err_code_ptr, target_sym->ptr, alir_const_int(ctx->module, 0)));
            
            AlirValue *err_code = new_temp(ctx, (VarType){TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, err_code, err_code_ptr, NULL));
            
            AlirValue *pristine_val_ptr = new_temp(ctx, pristine_type);
            pristine_val_ptr->type.ptr_depth++;
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, pristine_val_ptr, target_sym->ptr, alir_const_int(ctx->module, 1)));
            
            AlirValue *pristine_val = new_temp(ctx, pristine_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, pristine_val, pristine_val_ptr, NULL));
            
            emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, pristine_val, pristine_ptr));
            
            AlirValue *cond = new_temp(ctx, (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_EQ, cond, err_code, alir_const_int(ctx->module, 0)));
            
            AlirBlock *then_bb = alir_add_block(ctx->module, ctx->current_func, "clean_then");
            AlirBlock *else_bb = cn->residue_body ? alir_add_block(ctx->module, ctx->current_func, "clean_residue") : NULL;
            AlirBlock *merge_bb = alir_add_block(ctx->module, ctx->current_func, "clean_merge");
            
            AlirBlock *target_else = else_bb ? else_bb : merge_bb;
            
            AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, cond, alir_val_label(ctx->module, then_bb->label));
            br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
            br->args[0] = alir_val_label(ctx->module, target_else->label);
            br->arg_count = 1;
            emit(ctx, br);
            
            ctx->current_block = then_bb;
            ASTNode *s = cn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
            if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
                emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, merge_bb->label), NULL));
            }
            
            if (else_bb) {
                ctx->current_block = else_bb;
                
                VarType err_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
                AlirValue *err_ptr = new_temp(ctx, err_type);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, err_ptr, NULL, NULL));
                alir_add_symbol(ctx, cn->err_var_name, err_ptr, err_type);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, err_code, err_ptr));
                
                s = cn->residue_body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
                if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, merge_bb->label), NULL));
                }
            }
            
            ctx->current_block = merge_bb;
            break;
        }
        case NODE_UNTAINT: {
            UntaintNode *un = (UntaintNode*)node;
            AlirSymbol *target_sym = alir_find_symbol(ctx, un->var_name);
            
            VarType pristine_type = target_sym->type;
            pristine_type.is_tainted = 0;
            AlirValue *pristine_ptr = new_temp(ctx, pristine_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, pristine_ptr, NULL, NULL));
            
            // Add symbol using the SAME name (shadowing it)
            alir_add_symbol(ctx, un->var_name, pristine_ptr, pristine_type);
            
            AlirValue *err_code_ptr = new_temp(ctx, (VarType){TYPE_INT, 1, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, err_code_ptr, target_sym->ptr, alir_const_int(ctx->module, 0)));
            
            AlirValue *err_code = new_temp(ctx, (VarType){TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, err_code, err_code_ptr, NULL));
            
            AlirValue *pristine_val_ptr = new_temp(ctx, pristine_type);
            pristine_val_ptr->type.ptr_depth++;
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, pristine_val_ptr, target_sym->ptr, alir_const_int(ctx->module, 1)));
            
            AlirValue *pristine_val = new_temp(ctx, pristine_type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, pristine_val, pristine_val_ptr, NULL));
            
            emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, pristine_val, pristine_ptr));
            
            AlirValue *cond = new_temp(ctx, (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_EQ, cond, err_code, alir_const_int(ctx->module, 0)));
            
            AlirBlock *residue_bb = un->residue_body ? alir_add_block(ctx->module, ctx->current_func, "untaint_residue") : NULL;
            AlirBlock *merge_bb = alir_add_block(ctx->module, ctx->current_func, "untaint_merge");
            
            AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, cond, alir_val_label(ctx->module, merge_bb->label));
            br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
            br->args[0] = alir_val_label(ctx->module, residue_bb ? residue_bb->label : merge_bb->label);
            br->arg_count = 1;
            emit(ctx, br);
            
            if (residue_bb) {
                ctx->current_block = residue_bb;
                
                VarType err_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
                AlirValue *err_ptr = new_temp(ctx, err_type);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, err_ptr, NULL, NULL));
                alir_add_symbol(ctx, un->err_var_name, err_ptr, err_type);
                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, err_code, err_ptr));
                
                ASTNode *s = un->residue_body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
                if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, merge_bb->label), NULL));
                }
            }
            
            ctx->current_block = merge_bb;
            break;
        }
        case NODE_ERRNUM:
            break;

        case NODE_SIZEOF:
            alir_gen_expr(ctx, node);
            break;
        case NODE_META:
        case NODE_POSTMETA: {
            // Save current state
            AlirFunction *old_func = ctx->current_func;
            AlirBlock *old_block = ctx->current_block;

            // Create temporary compile-time ALIR function
            AlirFunction *meta_func = calloc(1, sizeof(AlirFunction));
            meta_func->name = "meta_compile_time";
            meta_func->blocks = alir_add_block(ctx->module, meta_func, "entry");

            ctx->current_func = meta_func;
            ctx->current_block = meta_func->blocks;

            // Snapshot globals before meta block
            AlirGlobal *old_globals = ctx->module->globals;

            MetaNode *mn = (MetaNode*)node;
            ASTNode *curr = mn->body;
            while(curr) {
                alir_gen_stmt(ctx, curr);
                curr = curr->next;
            }

            // Execute the meta block
            Arena meta_arena;
            arena_init(&meta_arena);
            MetaVM *vm = meta_vm_init(&meta_arena);
            int meta_err = meta_vm_execute(vm, ctx->module, meta_func, ctx->sem, NULL, 0);

            if (meta_err) {
                // The VM already reported the specific error at the exact line via sem_error.
                // But just in case, we also ensure error_count increases here if not caught:
                if (ctx->sem && ctx->sem->compiler_ctx) {
                    // Do not increment if sem_error already did it inside the VM,
                    // actually sem_error increments it! So we don't need to do anything else.
                }
            }

            // Clean up the global constants added by the meta block so they don't persist in ALIR
            ctx->module->globals = old_globals;

            meta_vm_free(vm);
            arena_reset(&meta_arena);

            // Restore state
            ctx->current_func = old_func;
            ctx->current_block = old_block;
            break;
        }
        case NODE_VAR_DECL: {
            alir_stmt_vardecl(ctx, node);
            break;
        }
        case NODE_ASSIGN: {
            alir_stmt_assign(ctx, node);
            break;
        }
        case NODE_SWITCH: alir_gen_switch(ctx, (SwitchNode*)node); break;
        case NODE_EMIT: alir_gen_flux_yield(ctx, (EmitNode*)node); break;

        case NODE_WHILE: {
            alir_stmt_while(ctx, node);
            break;
        }

        case NODE_LOOP: {
            LoopNode *ln = (LoopNode*)node;
            AlirBlock *body_bb = alir_add_block(ctx->module, ctx->current_func, "loop_body");
            AlirBlock *end_bb = alir_add_block(ctx->module, ctx->current_func, "loop_end");

            emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, body_bb->label), NULL));

            ctx->current_block = body_bb;
            push_loop(ctx, body_bb, end_bb);

            ASTNode *s = ln->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
            pop_loop(ctx);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, body_bb->label), NULL));

            ctx->current_block = end_bb;
            break;
        }

        case NODE_FOR_IN: {
            alir_stmt_for_in(ctx, node);
            break;
        }

        case NODE_BREAK:
            if (ctx->loop_break) emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, ctx->loop_break->label), NULL));
            break;

        case NODE_CONTINUE:
            if (ctx->loop_continue) emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, ctx->loop_continue->label), NULL));
            break;

        case NODE_RETURN: {
            ReturnNode *rn = (ReturnNode*)node;
            if (ctx->in_flux_resume) {
                AlirValue *fin_ptr = new_temp(ctx, (VarType){TYPE_BOOL, 1});
                emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, fin_ptr, ctx->flux_ctx_ptr, alir_const_int(ctx->module, 1)));
                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, alir_const_int(ctx->module, 1), fin_ptr));
                emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, NULL, NULL));
            } else {
                AlirValue *v = NULL;
                if (rn->value) {
                    v = alir_gen_expr(ctx, rn->value);
                    if (!v) v = alir_const_int(ctx->module, 0); // Safety net
                }
                emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, v, NULL));
            }
            break;
        }

        case NODE_CALL:
        case NODE_METHOD_CALL:
        case NODE_VAR_REF:
        case NODE_BINARY_OP:
        case NODE_UNARY_OP:
        case NODE_LITERAL:
        case NODE_ARRAY_LIT:
        case NODE_INDEX_ACCESS:
        case NODE_VECTOR_ACCESS:
        case NODE_MEMBER_ACCESS:
        case NODE_TYPEOF:
        case NODE_ALIGNOF:
        case NODE_DEFINED:
        case NODE_HAS_METHOD:
        case NODE_HAS_ATTRIBUTE:
        case NODE_CAST:
        case NODE_INC_DEC:
        case NODE_COMPOUND:
        case NODE_TEMPLATE_INSTANTIATION:
            alir_gen_expr(ctx, node);
            break;

        case NODE_DEFER:
            // TODO: Implement proper defer queueing for scope exits
            break;
        case NODE_IF: {
            IfNode *in = (IfNode*)node;
            AlirValue *cond = alir_gen_expr(ctx, in->condition);
            if (!cond) cond = alir_const_int(ctx->module, 0); // Safety net

            AlirBlock *then_bb = alir_add_block(ctx->module, ctx->current_func, "then");
            AlirBlock *else_bb = in->else_body ? alir_add_block(ctx->module, ctx->current_func, "else") : NULL;
            AlirBlock *merge_bb = alir_add_block(ctx->module, ctx->current_func, "merge");

            AlirBlock *target_else = else_bb ? else_bb : merge_bb;

            AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, cond, alir_val_label(ctx->module, then_bb->label));
            br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
            br->args[0] = alir_val_label(ctx->module, target_else->label);
            br->arg_count = 1;
            emit(ctx, br);

            ctx->current_block = then_bb;
            ASTNode *s = in->then_body; while(s){ alir_gen_stmt(ctx,s); s=s->next; }

            if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
                emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, merge_bb->label), NULL));
            }

            if (else_bb) {
                ctx->current_block = else_bb;
                s = in->else_body; while(s){ alir_gen_stmt(ctx,s); s=s->next; }
                if (!ctx->current_block->tail || !is_terminator(ctx->current_block->tail->op)) {
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, merge_bb->label), NULL));
                }
            }

            ctx->current_block = merge_bb;
            break;
        }

        case NODE_ROOT:
        case NODE_FUNC_DEF:
        case NODE_CLASS:
        case NODE_STRUCT:
        case NODE_NAMESPACE:
        case NODE_ENUM:
        case NODE_LINK:
        case NODE_CASE:
        case NODE_WASH:
        case NODE_NAMED_ARG:
            break;
        default:
            break;
    }
}
