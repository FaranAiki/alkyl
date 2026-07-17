#include "translate.h"
#include "../semantic/semantic.h"

void translate_inst(CodegenCtx *ctx, AlirInst *inst) {
    LLVMValueRef op1 = get_llvm_value(ctx, inst->op1);
    LLVMValueRef op2 = get_llvm_value(ctx, inst->op2);
    LLVMValueRef res = NULL;

    int is_float = (inst->op1 && (inst->op1->type.base == TYPE_FLOAT || inst->op1->type.base == TYPE_DOUBLE));

    switch (inst->op) {
        case ALIR_OP_ALLOCA: {
            LLVMTypeRef ty = get_llvm_type(ctx, inst->dest->type);
            if (op1) {
                LLVMValueRef size = op1;
                // alloca expects size_t (i64 on 64-bit systems)
                if (LLVMGetTypeKind(LLVMTypeOf(size)) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(LLVMTypeOf(size)) < 64) {
                    size = LLVMBuildZExt(ctx->builder, size, LLVMInt64TypeInContext(ctx->llvm_ctx), "sz_ext");
                }
                res = LLVMBuildArrayAlloca(ctx->builder, LLVMInt8TypeInContext(ctx->llvm_ctx), size, "alloc");
            } else {
                res = LLVMBuildAlloca(ctx->builder, ty, "alloc");
            }
            break;
        }
        // TODO fix this store
        case ALIR_OP_STORE: {
            if (op1 && op2) {
                LLVMValueRef val = op1;
                LLVMValueRef ptr = op2;
                
                if (LLVMGetTypeKind(LLVMTypeOf(ptr)) != LLVMPointerTypeKind) {
                    ptr = LLVMBuildIntToPtr(ctx->builder, ptr, LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0), "store_cast");
                }
                LLVMBuildStore(ctx->builder, val, ptr);
            }
            break;
        }
        case ALIR_OP_LOAD: {
            if (op1) {
                LLVMTypeRef ty = get_llvm_type(ctx, inst->dest->type);
                if (LLVMGetTypeKind(LLVMTypeOf(op1)) != LLVMPointerTypeKind) {
                    op1 = LLVMBuildIntToPtr(ctx->builder, op1, LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0), "load_cast");
                }
                res = LLVMBuildLoad2(ctx->builder, ty, op1, "load");
            }
            break;
        }
        // TODO load the res first then
        case ALIR_OP_GET_PTR: {
            if (!op1) break;
            
            // Validate GEP input strictly
      
            if (LLVMGetTypeKind(LLVMTypeOf(op1)) != LLVMPointerTypeKind) {
                op1 = LLVMBuildIntToPtr(ctx->builder, op1, LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0), "safe_cast");
            }

            VarType ptr_t = inst->op1->type;
            if (ptr_t.ptr_depth > 0) ptr_t.ptr_depth--;
            else if (ptr_t.array_size > 0) ptr_t.array_size = 0; // Natural Array decay
            
            LLVMTypeRef base_ty = get_llvm_type(ctx, ptr_t);
            
            // Differentiate Struct GEP (Constant Index) vs Array GEP
            if (ptr_t.base == TYPE_CLASS && ptr_t.ptr_depth == 0 && inst->op2 && inst->op2->kind == ALIR_VAL_CONST) {
                AlirStruct *st = alir_find_struct(ctx->alir_mod, ptr_t.class_name);
                if (st && st->is_union) {
                    res = op1;
                } else {
                    res = LLVMBuildStructGEP2(ctx->builder, base_ty, op1, (unsigned)inst->op2->val.int_val, "struct_gep");
                }
            } else {
                if (inst->op1->type.array_size > 0) {
                    // Proper LLVM GEP indexing for explicit Array types ([N x i32]*)
                    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0);
                    LLVMValueRef indices[] = { zero, op2 };
                    LLVMTypeRef arr_ty = get_llvm_type(ctx, inst->op1->type);
                    res = LLVMBuildGEP2(ctx->builder, arr_ty, op1, indices, 2, "array_gep");
                } else {
                    // Standard Pointer iteration (i32*)
                    LLVMValueRef indices[] = { op2 };
                    res = LLVMBuildGEP2(ctx->builder, base_ty, op1, indices, 1, "ptr_gep");
                }
            }
            break;
        }
        
        // Math Ops
        case ALIR_OP_ADD:
        case ALIR_OP_SUB:
        case ALIR_OP_MUL:
        case ALIR_OP_FADD:
        case ALIR_OP_FSUB:
        case ALIR_OP_FMUL: {
            LLVMValueRef act1 = op1;
            if (inst->op1 && inst->op1->type.is_tainted && LLVMGetTypeKind(LLVMTypeOf(op1)) == LLVMStructTypeKind && LLVMCountStructElementTypes(LLVMTypeOf(op1)) > 1) act1 = LLVMBuildExtractValue(ctx->builder, op1, 1, "ext1");
            LLVMValueRef act2 = op2;
            if (inst->op2 && inst->op2->type.is_tainted && LLVMGetTypeKind(LLVMTypeOf(op2)) == LLVMStructTypeKind && LLVMCountStructElementTypes(LLVMTypeOf(op2)) > 1) act2 = LLVMBuildExtractValue(ctx->builder, op2, 1, "ext2");
            
            // Fix ptr + int
            if (inst->op == ALIR_OP_ADD || inst->op == ALIR_OP_SUB) {
                if (LLVMGetTypeKind(LLVMTypeOf(act1)) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMTypeOf(act2)) == LLVMIntegerTypeKind) {
                    LLVMValueRef ptr_as_int = LLVMBuildPtrToInt(ctx->builder, act1, LLVMTypeOf(act2), "ptr2int");
                    LLVMValueRef math_res = inst->op == ALIR_OP_ADD ? LLVMBuildAdd(ctx->builder, ptr_as_int, act2, "ptr_add") : LLVMBuildSub(ctx->builder, ptr_as_int, act2, "ptr_sub");
                    res = LLVMBuildIntToPtr(ctx->builder, math_res, LLVMTypeOf(act1), "int2ptr");
                    break;
                }
            }

            switch(inst->op) {
                case ALIR_OP_ADD: res = LLVMBuildAdd(ctx->builder, act1, act2, "add"); break;
                case ALIR_OP_SUB: res = LLVMBuildSub(ctx->builder, act1, act2, "sub"); break;
                case ALIR_OP_MUL: res = LLVMBuildMul(ctx->builder, act1, act2, "mul"); break;
                case ALIR_OP_FADD: res = LLVMBuildFAdd(ctx->builder, act1, act2, "fadd"); break;
                case ALIR_OP_FSUB: res = LLVMBuildFSub(ctx->builder, act1, act2, "fsub"); break;
                case ALIR_OP_FMUL: res = LLVMBuildFMul(ctx->builder, act1, act2, "fmul"); break;
                default: break;
            }
            break;
        }
        case ALIR_OP_DIV:
        case ALIR_OP_MOD:
        case ALIR_OP_FDIV: {
            int is_float = (inst->op == ALIR_OP_FDIV);
            LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(ctx->builder);
            LLVMValueRef current_func = LLVMGetBasicBlockParent(current_bb);
            
            LLVMBasicBlockRef div_ok_bb = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, current_func, "div_ok");
            LLVMBasicBlockRef div_zero_bb = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, current_func, "div_zero");
            
            LLVMValueRef zero;
            LLVMValueRef cmp;
            if (is_float) {
                zero = LLVMConstReal(LLVMTypeOf(op2), 0.0);
                cmp = LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, op2, zero, "div_cmp");
            } else {
                zero = LLVMConstInt(LLVMTypeOf(op2), 0, 0);
                cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, op2, zero, "div_cmp");
            }
            LLVMBuildCondBr(ctx->builder, cmp, div_zero_bb, div_ok_bb);
            
            LLVMPositionBuilderAtEnd(ctx->builder, div_zero_bb);
            LLVMValueRef puts_func = LLVMGetNamedFunction(ctx->llvm_mod, "puts");
            if (!puts_func) {
                LLVMTypeRef args[] = { LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0) };
                LLVMTypeRef puts_ty = LLVMFunctionType(LLVMInt32TypeInContext(ctx->llvm_ctx), args, 1, 0);
                puts_func = LLVMAddFunction(ctx->llvm_mod, "puts", puts_ty);
            }
            LLVMValueRef exit_func = LLVMGetNamedFunction(ctx->llvm_mod, "exit");
            if (!exit_func) {
                LLVMTypeRef args[] = { LLVMInt32TypeInContext(ctx->llvm_ctx) };
                LLVMTypeRef exit_ty = LLVMFunctionType(LLVMVoidTypeInContext(ctx->llvm_ctx), args, 1, 0);
                exit_func = LLVMAddFunction(ctx->llvm_mod, "exit", exit_ty);
            }
            LLVMValueRef msg = LLVMBuildGlobalStringPtr(ctx->builder, "purge: ErrDivisionByZero", "div_zero_msg");
            LLVMValueRef args[] = { msg };
            LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(puts_func), puts_func, args, 1, "");
            LLVMValueRef args_exit[] = { LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 1, 0) };
            LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(exit_func), exit_func, args_exit, 1, "");
            LLVMBuildUnreachable(ctx->builder);
            
            LLVMPositionBuilderAtEnd(ctx->builder, div_ok_bb);
            if (inst->op == ALIR_OP_DIV) res = LLVMBuildSDiv(ctx->builder, op1, op2, "div");
            else if (inst->op == ALIR_OP_MOD) res = LLVMBuildSRem(ctx->builder, op1, op2, "mod");
            else res = LLVMBuildFDiv(ctx->builder, op1, op2, "fdiv");
            break;
        }
        
        // Logical
        case ALIR_OP_AND: res = LLVMBuildAnd(ctx->builder, op1, op2, "and"); break;
        case ALIR_OP_OR:  res = LLVMBuildOr(ctx->builder, op1, op2, "or"); break;
        case ALIR_OP_XOR: res = LLVMBuildXor(ctx->builder, op1, op2, "xor"); break;
        case ALIR_OP_SHL: res = LLVMBuildShl(ctx->builder, op1, op2, "shl"); break;
        case ALIR_OP_SHR: res = LLVMBuildAShr(ctx->builder, op1, op2, "shr"); break;
        case ALIR_OP_NOT: res = (LLVMGetTypeKind(LLVMTypeOf(op1)) == LLVMPointerTypeKind) ? LLVMBuildIsNull(ctx->builder, op1, "isnull") : LLVMBuildNot(ctx->builder, op1, "not"); break;
        
        // Comparisons
        case ALIR_OP_EQ:
        case ALIR_OP_NEQ: {
            if (op1 && op2) {
                LLVMValueRef act1 = op1;
                if (inst->op1->type.is_tainted && LLVMGetTypeKind(LLVMTypeOf(op1)) == LLVMStructTypeKind && LLVMCountStructElementTypes(LLVMTypeOf(op1)) > 1) act1 = LLVMBuildExtractValue(ctx->builder, op1, 1, "ext1");
                LLVMValueRef act2 = op2;
                if (inst->op2->type.is_tainted && LLVMGetTypeKind(LLVMTypeOf(op2)) == LLVMStructTypeKind && LLVMCountStructElementTypes(LLVMTypeOf(op2)) > 1) act2 = LLVMBuildExtractValue(ctx->builder, op2, 1, "ext2");

                
                LLVMTypeRef t1 = LLVMTypeOf(act1);
                LLVMTypeRef t2 = LLVMTypeOf(act2);
                if (LLVMGetTypeKind(t1) != LLVMGetTypeKind(t2)) {
                    if (LLVMGetTypeKind(t1) == LLVMPointerTypeKind && LLVMGetTypeKind(t2) == LLVMIntegerTypeKind) {
                        act2 = LLVMBuildIntToPtr(ctx->builder, act2, t1, "ptr_cast");
                    } else if (LLVMGetTypeKind(t2) == LLVMPointerTypeKind && LLVMGetTypeKind(t1) == LLVMIntegerTypeKind) {
                        act1 = LLVMBuildIntToPtr(ctx->builder, act1, t2, "ptr_cast");
                    }
                }
                res = is_float ? LLVMBuildFCmp(ctx->builder, (inst->op == ALIR_OP_EQ ? LLVMRealOEQ : LLVMRealONE), act1, act2, "feq") : 
                                LLVMBuildICmp(ctx->builder, (inst->op == ALIR_OP_EQ ? LLVMIntEQ : LLVMIntNE), act1, act2, "ieq");
                
                if (inst->dest && inst->dest->type.is_tainted) {
                    LLVMValueRef err_id = NULL;
                    if (inst->op1->type.is_tainted) err_id = LLVMBuildExtractValue(ctx->builder, op1, 0, "err1");
                    else if (inst->op2->type.is_tainted) err_id = LLVMBuildExtractValue(ctx->builder, op2, 0, "err2");
                    else err_id = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0);
                    
                    LLVMValueRef tainted_res = LLVMGetUndef(get_llvm_type(ctx, inst->dest->type));
                    tainted_res = LLVMBuildInsertValue(ctx->builder, tainted_res, err_id, 0, "ins_err");
                    res = LLVMBuildInsertValue(ctx->builder, tainted_res, res, 1, "ins_val");
                }
            }
            break;
        }
        case ALIR_OP_LT:  
        case ALIR_OP_GT:  
        case ALIR_OP_LTE: 
        case ALIR_OP_GTE: {
            LLVMValueRef act1 = op1;
            if (inst->op1 && inst->op1->type.is_tainted && LLVMGetTypeKind(LLVMTypeOf(op1)) == LLVMStructTypeKind && LLVMCountStructElementTypes(LLVMTypeOf(op1)) > 1) act1 = LLVMBuildExtractValue(ctx->builder, op1, 1, "ext1");
            LLVMValueRef act2 = op2;
            if (inst->op2 && inst->op2->type.is_tainted && LLVMGetTypeKind(LLVMTypeOf(op2)) == LLVMStructTypeKind && LLVMCountStructElementTypes(LLVMTypeOf(op2)) > 1) act2 = LLVMBuildExtractValue(ctx->builder, op2, 1, "ext2");
            
            switch (inst->op) {
                case ALIR_OP_LT:  res = is_float ? LLVMBuildFCmp(ctx->builder, LLVMRealOLT, act1, act2, "flt") : LLVMBuildICmp(ctx->builder, LLVMIntSLT, act1, act2, "ilt"); break;
                case ALIR_OP_GT:  res = is_float ? LLVMBuildFCmp(ctx->builder, LLVMRealOGT, act1, act2, "fgt") : LLVMBuildICmp(ctx->builder, LLVMIntSGT, act1, act2, "igt"); break;
                case ALIR_OP_LTE: res = is_float ? LLVMBuildFCmp(ctx->builder, LLVMRealOLE, act1, act2, "fle") : LLVMBuildICmp(ctx->builder, LLVMIntSLE, act1, act2, "ile"); break;
                case ALIR_OP_GTE: res = is_float ? LLVMBuildFCmp(ctx->builder, LLVMRealOGE, act1, act2, "fge") : LLVMBuildICmp(ctx->builder, LLVMIntSGE, act1, act2, "ige"); break;
                default: break;
            }
            break;
        }

        // Flow Control
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
        // TODO maybe use a type caster (?)
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
            for(int i = 0; i < inst->arg_count; i++) {
                args[i] = get_llvm_value(ctx, inst->args[i]);
                if (!args[i]) {
                    // Safety for unresolved arguments
                    args[i] = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
                }
                
                // CRITICAL: Prevent truncation for varargs on 64-bit systems.
                // If the argument is an integer, promote to 64-bit word size if it's potentially a pointer.
                // TODO fix this shitttttttttttttt
                LLVMTypeRef arg_ty = LLVMTypeOf(args[i]);
                if (LLVMGetTypeKind(arg_ty) == LLVMIntegerTypeKind) {
                    if (inst->args[i]->type.base == TYPE_UNKNOWN || inst->args[i]->type.base == TYPE_AUTO) {
                         if (LLVMGetIntTypeWidth(arg_ty) < 64) {
                             args[i] = LLVMBuildZExt(ctx->builder, args[i], LLVMInt64TypeInContext(ctx->llvm_ctx), "prom_word");
                         }
                    } else if (LLVMGetIntTypeWidth(arg_ty) < 32) {
                        args[i] = LLVMBuildSExt(ctx->builder, args[i], LLVMInt32TypeInContext(ctx->llvm_ctx), "prom_i32");
                    } else if (LLVMGetIntTypeWidth(arg_ty) >= 64) {
                        // printf("true!\n");
                        args[i] = LLVMBuildSExt(ctx->builder, args[i], LLVMInt64TypeInContext(ctx->llvm_ctx), "prom_i64");
                    }
                    // printf("%d\n", (LLVMGetIntTypeWidth(arg_ty)));
                // Force conversion from integer to double or whatnot
                }

            }

            res = LLVMBuildCall2(ctx->builder, func_ty, func, args, inst->arg_count, (LLVMGetReturnType(func_ty) == LLVMVoidTypeInContext(ctx->llvm_ctx)) ? "" : "call");
            LLVMSetInstructionCallConv(res, LLVMGetFunctionCallConv(func));
            free(args);
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
            // op1 is the tainted struct { i32 err_id, base }, op2 is the fallback value
            
            if (LLVMGetTypeKind(LLVMTypeOf(op1)) != LLVMStructTypeKind) {
                // op1 is already pristine, no error can exist. Ignore fallback.
                res = op1;
                break;
            }
            
            LLVMValueRef err_id = LLVMBuildExtractValue(ctx->builder, op1, 0, "err_id");
            LLVMValueRef val = LLVMCountStructElementTypes(LLVMTypeOf(op1)) > 1 ? LLVMBuildExtractValue(ctx->builder, op1, 1, "val") : LLVMConstNull(LLVMInt32TypeInContext(ctx->llvm_ctx));
            
            LLVMValueRef has_err = LLVMBuildICmp(ctx->builder, LLVMIntNE, err_id, LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0), "has_err");
            
            // Branch to select op2 if has_err, else val
            // We can just use a select instruction instead of branches!
            // Wait, op2 might have side effects, but ALIR generates them sequentially, so op2 is already evaluated here!
            res = LLVMBuildSelect(ctx->builder, has_err, op2, val, "fallback_res");
            break;
        }
        case ALIR_OP_PANIC: {
            LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(ctx->builder);
            if (LLVMGetBasicBlockTerminator(current_bb) != NULL) break;
            LLVMValueRef current_func = LLVMGetBasicBlockParent(current_bb);
            LLVMTypeRef ret_ty = LLVMGetReturnType(LLVMGlobalGetValueType(current_func));
            
            if (LLVMGetTypeKind(ret_ty) == LLVMStructTypeKind) {
                LLVMValueRef ret_struct = LLVMGetUndef(ret_ty);
                ret_struct = LLVMBuildInsertValue(ctx->builder, ret_struct, LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 1, 0), 0, "");
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
        
        // Conversions and Casts
        case ALIR_OP_BITCAST: {
            if (op1) {
                LLVMTypeRef dest_ty = get_llvm_type(ctx, inst->dest->type);
                LLVMTypeRef src_ty = LLVMTypeOf(op1);
                LLVMTypeKind op1_k = LLVMGetTypeKind(src_ty);
                LLVMTypeKind dest_k = LLVMGetTypeKind(dest_ty);

                if (op1_k == dest_k) {
                    res = op1; 
                } else if (op1_k == LLVMIntegerTypeKind && dest_k == LLVMPointerTypeKind) {
                    res = LLVMBuildIntToPtr(ctx->builder, op1, dest_ty, "inttoptr");
                } else if (op1_k == LLVMPointerTypeKind && dest_k == LLVMIntegerTypeKind) {
                    res = LLVMBuildPtrToInt(ctx->builder, op1, dest_ty, "ptrtoint");
                } else if (op1_k == LLVMPointerTypeKind && dest_k == LLVMPointerTypeKind) {
                    res = LLVMBuildBitCast(ctx->builder, op1, dest_ty, "ptr_bitcast");
                } else {
                    // Last resort, might still fail LLVM verify if types are incompatible sizes
                    res = LLVMBuildBitCast(ctx->builder, op1, dest_ty, "bitcast");
                }
            }
            break;
        }

        case ALIR_OP_CAST: {
            if (!op1) break;
            
            LLVMTypeRef dest_ty = get_llvm_type(ctx, inst->dest->type);
            LLVMTypeRef src_ty = LLVMTypeOf(op1);
            LLVMTypeKind src_k = LLVMGetTypeKind(src_ty);
            LLVMTypeKind dest_k = LLVMGetTypeKind(dest_ty);

            LLVMValueRef actual_op1 = op1;
            LLVMValueRef err_id = NULL;
            int is_src_tainted = inst->op1->type.is_tainted;
            int is_dest_tainted = inst->dest->type.is_tainted;
            
            if (is_src_tainted && src_k == LLVMStructTypeKind) {
                err_id = LLVMBuildExtractValue(ctx->builder, op1, 0, "ext_err");
                if (LLVMCountStructElementTypes(src_ty) > 1) {
                    actual_op1 = LLVMBuildExtractValue(ctx->builder, op1, 1, "ext_val");
                }
                src_ty = LLVMTypeOf(actual_op1);
                src_k = LLVMGetTypeKind(src_ty);
            }
            
            LLVMTypeRef inner_dest_ty = dest_ty;
            if (is_dest_tainted && dest_k == LLVMStructTypeKind) {
                inner_dest_ty = LLVMStructGetTypeAtIndex(dest_ty, 1);
            }
            
            LLVMTypeKind inner_dest_k = LLVMGetTypeKind(inner_dest_ty);

            LLVMValueRef cast_res = NULL;
            if (is_float) {
                if (inst->dest->type.base == TYPE_FLOAT || inst->dest->type.base == TYPE_DOUBLE) {
                    cast_res = LLVMBuildFPCast(ctx->builder, actual_op1, inner_dest_ty, "fpcast");
                } else {
                    cast_res = LLVMBuildFPToSI(ctx->builder, actual_op1, inner_dest_ty, "fptosi");
                }
            } else {
                if (src_k == LLVMPointerTypeKind || dest_k == LLVMPointerTypeKind) {
                    if (src_k == LLVMPointerTypeKind && dest_k == LLVMPointerTypeKind) {
                        cast_res = LLVMBuildBitCast(ctx->builder, actual_op1, inner_dest_ty, "ptr_bitcast");
                    } else if (src_k == LLVMPointerTypeKind && dest_k == LLVMIntegerTypeKind) {
                        cast_res = LLVMBuildPtrToInt(ctx->builder, actual_op1, inner_dest_ty, "ptrtoint");
                    } else if (src_k == LLVMIntegerTypeKind && dest_k == LLVMPointerTypeKind) {
                        cast_res = LLVMBuildIntToPtr(ctx->builder, actual_op1, inner_dest_ty, "inttoptr");
                    } else if (src_k == LLVMPointerTypeKind && dest_k == LLVMStructTypeKind) {
                        cast_res = LLVMBuildLoad2(ctx->builder, inner_dest_ty, actual_op1, "struct_load");
                    } else {
                        cast_res = LLVMBuildBitCast(ctx->builder, actual_op1, inner_dest_ty, "cast_bitcast");
                    }
                } else if (inst->dest->type.base == TYPE_FLOAT || inst->dest->type.base == TYPE_DOUBLE) {
                    cast_res = LLVMBuildSIToFP(ctx->builder, actual_op1, inner_dest_ty, "sitofp");
                } else {
                    cast_res = LLVMBuildIntCast(ctx->builder, actual_op1, inner_dest_ty, "intcast");
                }
            }
            
            if (is_dest_tainted) {
                res = LLVMGetUndef(dest_ty);
                res = LLVMBuildInsertValue(ctx->builder, res, err_id ? err_id : LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0), 0, "ins_err");
                res = LLVMBuildInsertValue(ctx->builder, res, cast_res, 1, "ins_val");
            } else {
                res = cast_res;
            }

            break;
        }

        
        // Low Level Memory Overrides
        // TODO make sure this is proper
        case ALIR_OP_SIZEOF: {
            LLVMTypeRef ty = get_llvm_type(ctx, inst->op1->type);
            res = LLVMSizeOf(ty);
            LLVMTypeRef dest_ty = inst->dest ? get_llvm_type(ctx, inst->dest->type) : LLVMInt64TypeInContext(ctx->llvm_ctx);
            res = LLVMBuildIntCast(ctx->builder, res, dest_ty, "sz_cast");
            break;
        }
        case ALIR_OP_ALIGNOF: {
            LLVMTypeRef ty = get_llvm_type(ctx, inst->op1->type);
            res = LLVMAlignOf(ty);
            LLVMTypeRef dest_ty = inst->dest ? get_llvm_type(ctx, inst->dest->type) : LLVMInt64TypeInContext(ctx->llvm_ctx);
            res = LLVMBuildIntCast(ctx->builder, res, dest_ty, "al_cast");
            break;
        }
        case ALIR_OP_TYPEOF: {
            char *type_str = sem_type_to_str(inst->op1->type);
            res = LLVMBuildGlobalStringPtr(ctx->builder, type_str, "typeof_str");
            break;
        }
        case ALIR_OP_MOV: {
            res = op1; // Assign value directly (Register Aliasing)
            break;
        }

        // Native Abstract Iterators Lowering 
        case ALIR_OP_ITER_INIT: {
            llvm_codegen_flux_iter_init(ctx, inst, op1, &res);
            break;
        }
        case ALIR_OP_ITER_VALID: {
            llvm_codegen_flux_iter_valid(ctx, op1, &res);
            break;
        }
        case ALIR_OP_ITER_GET: {
            llvm_codegen_flux_iter_get(ctx, inst, op1, &res);
            break;
        }
        case ALIR_OP_ITER_NEXT: {
            llvm_codegen_flux_iter_next(ctx, op1); 
            break;
        }

        // Catch explicitly untranslated features or flux opcodes mapped out by ALIR
        // TODO alir free stack
        // ALIR_OP_YIELD todo <--- just use gemini for this shit fuck you I am too lazy coding
        // ALIR_OP_PHI todo
        default: break; 
    }

    if (inst->dest && res) {
        set_llvm_value(ctx, inst->dest, res);
    }

}
