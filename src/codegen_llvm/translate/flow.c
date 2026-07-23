#include "../../../include/codegen_llvm/translate.h"
#include "../../semantic/semantic.h"

LLVMValueRef translate_flow(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float) {
    (void)is_float;
    LLVMValueRef res = NULL;
    switch (inst->op) {
        case ALIR_OP_JUMP: {
            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) != NULL) break;
            LLVMBasicBlockRef dest_bb = hashmap_get(&ctx->block_map, inst->op1->val.str_val);
            if (dest_bb) LLVMBuildBr(ctx->builder, dest_bb);
            break;
        }
        case ALIR_OP_CONDI: {
            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) != NULL) break;
            LLVMBasicBlockRef then_bb = hashmap_get(&ctx->block_map, inst->op2->val.str_val);
            LLVMBasicBlockRef else_bb = hashmap_get(&ctx->block_map, inst->args[0]->val.str_val);
            if (then_bb && else_bb && op1) LLVMBuildCondBr(ctx->builder, op1, then_bb, else_bb);
            break;
        }
        case ALIR_OP_SWITCH: {
            LLVMBasicBlockRef default_bb = hashmap_get(&ctx->block_map, inst->op2->val.str_val);
            int num_cases = 0;
            for(AlirSwitchCase *c = inst->cases; c; c = c->next) num_cases++;
            
            if (op1 && default_bb) {
                res = LLVMBuildSwitch(ctx->builder, op1, default_bb, num_cases);
                for(AlirSwitchCase *c = inst->cases; c; c = c->next) {
                    LLVMValueRef case_val = LLVMConstInt(get_llvm_type(ctx, inst->op1->type), c->value, 0);
                    LLVMBasicBlockRef case_bb = hashmap_get(&ctx->block_map, c->label);
                    if (case_bb) LLVMAddCase(res, case_val, case_bb);
                }
            }
            break;
        }
        case ALIR_OP_CALL: {
            LLVMValueRef func = get_llvm_value(ctx, inst->op1);
            LLVMTypeRef func_ty = NULL;

            if (func && LLVMGetValueKind(func) == LLVMFunctionValueKind) {
                func_ty = LLVMGlobalGetValueType(func);
            } else if (func) {
                LLVMTypeRef ret_ty = inst->dest ? get_llvm_type(ctx, inst->dest->type) : LLVMVoidTypeInContext(ctx->llvm_ctx);
                int param_count = inst->arg_count;
                LLVMTypeRef *param_types = malloc(sizeof(LLVMTypeRef) * param_count);
                for (int i=0; i<param_count; i++) {
                    if (inst->args[i]) {
                        param_types[i] = get_llvm_type(ctx, inst->args[i]->type);
                    } else {
                        param_types[i] = LLVMInt64TypeInContext(ctx->llvm_ctx);
                    }
                }
                func_ty = LLVMFunctionType(ret_ty, param_types, param_count, inst->op1->type.fp_is_varargs);
                free(param_types);
            }

            if (!func && inst->op1 && (inst->op1->kind == ALIR_VAL_VAR || inst->op1->kind == ALIR_VAL_GLOBAL) && inst->op1->val.str_val) {
                func = hashmap_get(&ctx->func_map, inst->op1->val.str_val);
                func_ty = hashmap_get(&ctx->func_type_map, inst->op1->val.str_val);
                
                if (!func) {
                    LLVMTypeRef ret_ty = inst->dest ? get_llvm_type(ctx, inst->dest->type) : LLVMVoidTypeInContext(ctx->llvm_ctx);
                    func_ty = LLVMFunctionType(ret_ty, NULL, 0, 1); // Vararg fallback
                    func = LLVMAddFunction(ctx->llvm_mod, inst->op1->val.str_val, func_ty);
                }
            }

            if (!func) break;
            
            LLVMValueRef *args = malloc(sizeof(LLVMValueRef) * inst->arg_count);
            unsigned num_params = LLVMCountParamTypes(func_ty);
            LLVMTypeRef *param_tys = malloc(sizeof(LLVMTypeRef) * num_params);
            LLVMGetParamTypes(func_ty, param_tys);
            
            for(int i = 0; i < inst->arg_count; i++) {
                args[i] = get_llvm_value(ctx, inst->args[i]);
                if (!args[i]) {
                    // Safety for unresolved arguments
                    args[i] = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
                }
                
                // CRITICAL: Prevent truncation for varargs on 64-bit systems.
                LLVMTypeRef arg_ty = LLVMTypeOf(args[i]);
                if (LLVMGetTypeKind(arg_ty) == LLVMIntegerTypeKind) {
                    if (inst->args[i]->type.base == TYPE_UNKNOWN || inst->args[i]->type.base == TYPE_AUTO) {
                         if (LLVMGetIntTypeWidth(arg_ty) < 64) {
                             args[i] = LLVMBuildZExt(ctx->builder, args[i], LLVMInt64TypeInContext(ctx->llvm_ctx), "prom_word");
                         }
                    } else if (LLVMGetIntTypeWidth(arg_ty) < 32) {
                        args[i] = LLVMBuildSExt(ctx->builder, args[i], LLVMInt32TypeInContext(ctx->llvm_ctx), "prom_i32");
                    }
                }
                
                // If arg is tainted (struct) but function expects pristine (non-struct), extract inner value
                if ((unsigned)i < num_params) {
                    LLVMTypeRef expected_ty = param_tys[i];
                    if (LLVMGetTypeKind(expected_ty) != LLVMStructTypeKind && LLVMGetTypeKind(LLVMTypeOf(args[i])) == LLVMStructTypeKind) {
                        if (LLVMCountStructElementTypes(LLVMTypeOf(args[i])) > 1) {
                            args[i] = LLVMBuildExtractValue(ctx->builder, args[i], 1, "ext_taint_arg");
                        }
                    }
                }
            }
            free(param_tys);

            res = LLVMBuildCall2(ctx->builder, func_ty, func, args, inst->arg_count, (LLVMGetReturnType(func_ty) == LLVMVoidTypeInContext(ctx->llvm_ctx)) ? "" : "call");
            LLVMSetInstructionCallConv(res, LLVMGetFunctionCallConv(func));
            free(args);
            
            if (inst->dest) {
                LLVMTypeRef expected_res_ty = get_llvm_type(ctx, inst->dest->type);
                if (LLVMGetTypeKind(expected_res_ty) == LLVMStructTypeKind && LLVMGetTypeKind(LLVMTypeOf(res)) != LLVMStructTypeKind) {
                    LLVMValueRef wrapped = LLVMGetUndef(expected_res_ty);
                    wrapped = LLVMBuildInsertValue(ctx->builder, wrapped, LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0), 0, "wrap_err");
                    wrapped = LLVMBuildInsertValue(ctx->builder, wrapped, res, 1, "wrap_val");
                    res = wrapped;
                }
            }
            
            break;
        }
        case ALIR_OP_RET: {
            LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(ctx->builder);
            if (LLVMGetBasicBlockTerminator(current_bb) != NULL) break;
            LLVMValueRef current_func = LLVMGetBasicBlockParent(current_bb);
            LLVMTypeRef ret_ty = LLVMGetReturnType(LLVMGlobalGetValueType(current_func));
            
            if (op1) {
                if (LLVMGetTypeKind(ret_ty) == LLVMStructTypeKind && LLVMGetTypeKind(LLVMTypeOf(op1)) != LLVMStructTypeKind) {
                    LLVMValueRef ret_struct = LLVMGetUndef(ret_ty);
                    ret_struct = LLVMBuildInsertValue(ctx->builder, ret_struct, LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0), 0, "");
                    ret_struct = LLVMBuildInsertValue(ctx->builder, ret_struct, op1, 1, "");
                    LLVMBuildRet(ctx->builder, ret_struct);
                } else if (ret_ty == LLVMVoidTypeInContext(ctx->llvm_ctx)) {
                    LLVMBuildRetVoid(ctx->builder);
                } else {
                    LLVMBuildRet(ctx->builder, op1);
                }
            } else {
                if (LLVMGetTypeKind(ret_ty) == LLVMStructTypeKind) {
                    LLVMValueRef ret_struct = LLVMGetUndef(ret_ty);
                    ret_struct = LLVMBuildInsertValue(ctx->builder, ret_struct, LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0), 0, "");
                    LLVMBuildRet(ctx->builder, ret_struct);
                } else {
                    LLVMBuildRetVoid(ctx->builder);
                }
            }
            break;
        }
        case ALIR_OP_FALLBACK: {
            if (LLVMGetTypeKind(LLVMTypeOf(op1)) != LLVMStructTypeKind) {
                res = op1;
                break;
            }
            
            LLVMValueRef err_id = LLVMBuildExtractValue(ctx->builder, op1, 0, "err_id");
            LLVMValueRef val = LLVMCountStructElementTypes(LLVMTypeOf(op1)) > 1 ? LLVMBuildExtractValue(ctx->builder, op1, 1, "val") : LLVMConstNull(LLVMInt32TypeInContext(ctx->llvm_ctx));
            
            LLVMValueRef has_err;
            if (inst->arg_count > 0 && inst->args[0]) {
                LLVMValueRef target_err = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), inst->args[0]->val.int_val, 0);
                has_err = LLVMBuildICmp(ctx->builder, LLVMIntEQ, err_id, target_err, "has_target_err");
                
                LLVMValueRef new_err_id = LLVMBuildSelect(ctx->builder, has_err, LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0), err_id, "new_err_id");
                LLVMValueRef new_val = LLVMBuildSelect(ctx->builder, has_err, op2, val, "new_val");
                
                LLVMValueRef new_struct = LLVMGetUndef(LLVMTypeOf(op1));
                new_struct = LLVMBuildInsertValue(ctx->builder, new_struct, new_err_id, 0, "ins_err");
                if (LLVMCountStructElementTypes(LLVMTypeOf(op1)) > 1) {
                    new_struct = LLVMBuildInsertValue(ctx->builder, new_struct, new_val, 1, "ins_val");
                }
                res = new_struct;
            } else {
                has_err = LLVMBuildICmp(ctx->builder, LLVMIntNE, err_id, LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0), "has_err");
                res = LLVMBuildSelect(ctx->builder, has_err, op2, val, "fallback_res");
            }
            break;
        }
        case ALIR_OP_PANIC: {
            LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(ctx->builder);
            if (LLVMGetBasicBlockTerminator(current_bb) != NULL) break;
            LLVMValueRef current_func = LLVMGetBasicBlockParent(current_bb);
            LLVMTypeRef ret_ty = LLVMGetReturnType(LLVMGlobalGetValueType(current_func));
            
            LLVMValueRef err_id_val = op2 ? op2 : LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 1, 0);
            
            if (LLVMGetTypeKind(ret_ty) == LLVMStructTypeKind) {
                LLVMValueRef ret_struct = LLVMGetUndef(ret_ty);
                ret_struct = LLVMBuildInsertValue(ctx->builder, ret_struct, err_id_val, 0, "");
                LLVMBuildRet(ctx->builder, ret_struct);
                break;
            }

            LLVMValueRef printf_func = LLVMGetNamedFunction(ctx->llvm_mod, "printf");
            if (!printf_func) {
                LLVMTypeRef args[] = { LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0) };
                LLVMTypeRef printf_ty = LLVMFunctionType(LLVMInt32TypeInContext(ctx->llvm_ctx), args, 1, 1);
                printf_func = LLVMAddFunction(ctx->llvm_mod, "printf", printf_ty);
            }
            LLVMValueRef exit_func = LLVMGetNamedFunction(ctx->llvm_mod, "exit");
            if (!exit_func) {
                LLVMTypeRef args[] = { LLVMInt32TypeInContext(ctx->llvm_ctx) };
                LLVMTypeRef exit_ty = LLVMFunctionType(LLVMVoidTypeInContext(ctx->llvm_ctx), args, 1, 0);
                exit_func = LLVMAddFunction(ctx->llvm_mod, "exit", exit_ty);
            }
            
            if (op1) {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder, "purge: %s\n", "purge_fmt");
                LLVMValueRef args[] = { fmt, op1 };
                LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(printf_func), printf_func, args, 2, "printf_call");
            }
            LLVMValueRef args_exit[] = { LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 1, 0) };
            LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(exit_func), exit_func, args_exit, 1, "");
            
            break;
        }
        default: break;
    }
    return res;
}
