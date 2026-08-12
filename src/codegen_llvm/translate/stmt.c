#include "../../../include/codegen_llvm/translate.h"
#include "../../semantic/semantic.h"

LLVMValueRef translate_stmt(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float) {
    (void)is_float;
    LLVMValueRef res = NULL;
    switch (inst->op) {
            case ALIR_OP_ALLOCA: {
                VarType elem_type = inst->dest->type;
                LLVMTypeRef ty = get_llvm_type(ctx, elem_type);
                if (elem_type.base == 0 && elem_type.ptr_depth == 0) { // TYPE_VOID
                    res = NULL;
                    break;
                }
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
            case ALIR_OP_STORE: {
                if (op1 && op2) {
                    LLVMValueRef val = op1;
                    LLVMValueRef ptr = op2;
                    
                    if (LLVMGetTypeKind(LLVMTypeOf(ptr)) != LLVMPointerTypeKind) {
                        ptr = LLVMBuildIntToPtr(ctx->builder, ptr, LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0), "store_cast");
                    }
                    LLVMTypeRef pointed = NULL;
                    if (inst->op2) {
                        VarType ptr_t = inst->op2->type;
                        if (ptr_t.ptr_depth > 0) ptr_t.ptr_depth--;
                        else if (ptr_t.array_size > 0) ptr_t.array_size = 0;
                        pointed = get_llvm_type(ctx, ptr_t);
                    }
                    if (pointed && LLVMGetTypeKind(pointed) == LLVMStructTypeKind && LLVMCountStructElementTypes(pointed) > 1 &&
                        LLVMGetTypeKind(LLVMTypeOf(val)) != LLVMStructTypeKind &&
                        LLVMGetTypeKind(LLVMTypeOf(val)) != LLVMPointerTypeKind) {
                        debug_codegen("wrapping value in store for tainted struct\n");
                        LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0);
                        LLVMValueRef wrapped = LLVMGetUndef(pointed);
                        wrapped = LLVMBuildInsertValue(ctx->builder, wrapped, zero, 0, "wrap_err");
                        wrapped = LLVMBuildInsertValue(ctx->builder, wrapped, val, 1, "wrap_val");
                        LLVMBuildStore(ctx->builder, wrapped, ptr);
                        break;
                    }
                    if (pointed && LLVMGetTypeKind(pointed) == LLVMStructTypeKind && LLVMCountStructElementTypes(pointed) > 1 &&
                        LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMStructTypeKind &&
                        LLVMTypeOf(val) != pointed) {
                        LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder, val, 1, "ext_payload");
                        LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0);
                        LLVMValueRef wrapped = LLVMGetUndef(pointed);
                        wrapped = LLVMBuildInsertValue(ctx->builder, wrapped, zero, 0, "wrap_err");
                        wrapped = LLVMBuildInsertValue(ctx->builder, wrapped, payload, 1, "wrap_val");
                        LLVMBuildStore(ctx->builder, wrapped, ptr);
                        break;
                    }
                    LLVMBuildStore(ctx->builder, val, ptr);
                }
                break;
            }
            case ALIR_OP_LOAD: {
                if (op1) {
                    LLVMTypeRef ty;
                    if (inst->dest->type.ptr_depth > 0) {
                        ty = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
                    } else {
                        ty = get_llvm_type(ctx, inst->dest->type);
                    }
                    if (LLVMGetTypeKind(LLVMTypeOf(op1)) != LLVMPointerTypeKind) {
                        op1 = LLVMBuildIntToPtr(ctx->builder, op1, LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0), "load_cast");
                    }
                    res = LLVMBuildLoad2(ctx->builder, ty, op1, "load");
                }
                break;
            }
            case ALIR_OP_BITCAST: {
                if (op1) {
                    LLVMTypeRef ty;
                    if (inst->dest->type.ptr_depth > 0) {
                        ty = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
                    } else {
                        ty = get_llvm_type(ctx, inst->dest->type);
                    }
                    
                    if (LLVMGetTypeKind(ty) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMTypeOf(op1)) == LLVMPointerTypeKind) {
                        res = op1; // Opaque pointers: no bitcast needed!
                    } else {
                        res = LLVMBuildBitCast(ctx->builder, op1, ty, "bitcast");
                    }
                }
                break;
            }
            // TODO load the res first then
            case ALIR_OP_GET_PTR: {
                if (!op1) break;
                
                if (LLVMIsNull(op1)) break;
                
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
                        
                        VarType raw_t = inst->op1->type;
                        LLVMTypeRef arr_ty = get_llvm_type(ctx, raw_t);
                        
                        res = LLVMBuildGEP2(ctx->builder, arr_ty, op1, indices, 2, "array_gep");
                    } else {
                        // Standard Pointer iteration (i32*)
                        LLVMValueRef indices[] = { op2 };
                        res = LLVMBuildGEP2(ctx->builder, base_ty, op1, indices, 1, "ptr_gep");
                    }
                }
                break;
            }
        default:
            break;
    }
    return res;
}
