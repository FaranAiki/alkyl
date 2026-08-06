import re
with open("src/codegen_llvm/translate/expr.c", "r") as f:
    text = f.read()

bitcast_logic = """
            case ALIR_OP_BITCAST: {
                LLVMTypeRef ty;
                if (inst->dest->type.ptr_depth > 0) {
                    ty = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
                } else {
                    ty = get_llvm_type(ctx, inst->dest->type);
                }
                
                size_t src_sz = LLVMABISizeOfType(LLVMGetModuleDataLayout(ctx->llvm_mod), LLVMTypeOf(op1));
                size_t dst_sz = LLVMABISizeOfType(LLVMGetModuleDataLayout(ctx->llvm_mod), ty);
                
                LLVMTypeKind src_kind = LLVMGetTypeKind(LLVMTypeOf(op1));
                LLVMTypeKind dst_kind = LLVMGetTypeKind(ty);
                
                if (src_kind == LLVMStructTypeKind || src_kind == LLVMArrayTypeKind ||
                    dst_kind == LLVMStructTypeKind || dst_kind == LLVMArrayTypeKind ||
                    src_sz != dst_sz || inst->custom_flag != 0) {
                    
                    if (src_sz != dst_sz && src_kind == LLVMIntegerTypeKind && dst_kind == LLVMIntegerTypeKind) {
                        if (src_sz < dst_sz) {
                            LLVMTypeRef large_int_ty = LLVMIntTypeInContext(ctx->llvm_ctx, dst_sz * 8);
                            op1 = LLVMBuildZExt(ctx->builder, op1, large_int_ty, "zext");
                        } else {
                            LLVMTypeRef small_int_ty = LLVMIntTypeInContext(ctx->llvm_ctx, dst_sz * 8);
                            op1 = LLVMBuildTrunc(ctx->builder, op1, small_int_ty, "trunc");
                        }
                    } else {
                        // Memory bitcast
                        size_t max_sz = src_sz > dst_sz ? src_sz : dst_sz;
                        if (max_sz == 0) max_sz = 1;
                        LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, LLVMArrayType(LLVMInt8TypeInContext(ctx->llvm_ctx), max_sz), "pad");
                        LLVMValueRef ptr1 = LLVMBuildPointerCast(ctx->builder, alloca, LLVMPointerType(LLVMTypeOf(op1), 0), "p1");
                        LLVMBuildStore(ctx->builder, op1, ptr1);
                        
                        if (inst->custom_flag == 1 /* ENDIAN_LITTLE */ || inst->custom_flag == 2 /* ENDIAN_BIG */) {
                            int is_host_little = (LLVMByteOrder(LLVMGetModuleDataLayout(ctx->llvm_mod)) == LLVMLittleEndian);
                            int needs_swap = 0;
                            if (inst->custom_flag == 1 && !is_host_little) needs_swap = 1;
                            if (inst->custom_flag == 2 && is_host_little) needs_swap = 1;
                            
                            if (needs_swap) {
                                size_t byte_count = (src_sz > dst_sz ? src_sz : dst_sz);
                                if (byte_count > 1) {
                                    LLVMBasicBlockRef cur_bb = LLVMGetInsertBlock(ctx->builder);
                                    LLVMValueRef func = LLVMGetBasicBlockParent(cur_bb);
                                    LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, func, "bswap_loop");
                                    LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, func, "bswap_end");
                                    
                                    LLVMValueRef half_count = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), byte_count / 2, 0);
                                    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0);
                                    
                                    LLVMValueRef idx_alloca = LLVMBuildAlloca(ctx->builder, LLVMInt32TypeInContext(ctx->llvm_ctx), "idx");
                                    LLVMBuildStore(ctx->builder, zero, idx_alloca);
                                    LLVMBuildBr(ctx->builder, loop_bb);
                                    
                                    LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);
                                    LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, LLVMInt32TypeInContext(ctx->llvm_ctx), idx_alloca, "i");
                                    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntULT, idx, half_count, "cond");
                                    
                                    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, func, "bswap_body");
                                    LLVMBuildCondBr(ctx->builder, cond, body_bb, end_bb);
                                    
                                    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
                                    
                                    LLVMValueRef byte_ptr_ty = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
                                    LLVMValueRef base_ptr = LLVMBuildPointerCast(ctx->builder, alloca, byte_ptr_ty, "base");
                                    
                                    LLVMValueRef idx1 = idx;
                                    LLVMValueRef idx2 = LLVMBuildSub(ctx->builder, LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), byte_count - 1, 0), idx, "idx2");
                                    
                                    LLVMValueRef p1 = LLVMBuildGEP2(ctx->builder, LLVMInt8TypeInContext(ctx->llvm_ctx), base_ptr, &idx1, 1, "p1");
                                    LLVMValueRef p2 = LLVMBuildGEP2(ctx->builder, LLVMInt8TypeInContext(ctx->llvm_ctx), base_ptr, &idx2, 1, "p2");
                                    
                                    LLVMValueRef v1 = LLVMBuildLoad2(ctx->builder, LLVMInt8TypeInContext(ctx->llvm_ctx), p1, "v1");
                                    LLVMValueRef v2 = LLVMBuildLoad2(ctx->builder, LLVMInt8TypeInContext(ctx->llvm_ctx), p2, "v2");
                                    
                                    LLVMBuildStore(ctx->builder, v2, p1);
                                    LLVMBuildStore(ctx->builder, v1, p2);
                                    
                                    LLVMValueRef next_idx = LLVMBuildAdd(ctx->builder, idx, LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 1, 0), "next_i");
                                    LLVMBuildStore(ctx->builder, next_idx, idx_alloca);
                                    LLVMBuildBr(ctx->builder, loop_bb);
                                    
                                    LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
                                }
                            }
                        }
                        
                        LLVMValueRef ptr2 = LLVMBuildPointerCast(ctx->builder, alloca, LLVMPointerType(ty, 0), "p2");
                        res = LLVMBuildLoad2(ctx->builder, ty, ptr2, "loadpad");
                        goto bitcast_done;
                    }
                }
                
                if (LLVMGetTypeKind(ty) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMTypeOf(op1)) == LLVMPointerTypeKind) {
                    res = op1;
                } else {
                    res = LLVMBuildBitCast(ctx->builder, op1, ty, "bitcast");
                }
            bitcast_done:
                break;
            }
"""

start_str = "            case ALIR_OP_BITCAST: {"
end_str = "            case ALIR_OP_CAST: {"

start_idx = text.find(start_str)
end_idx = text.find(end_str)

if start_idx != -1 and end_idx != -1:
    text = text[:start_idx] + bitcast_logic.strip() + "\\n" + text[end_idx:]
    with open("src/codegen_llvm/translate/expr.c", "w") as f:
        f.write(text)

