#include "../../../include/codegen_llvm/translate.h"
#include "../../semantic/semantic.h"

LLVMValueRef translate_expr(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float) {
    LLVMValueRef res = NULL;
    switch (inst->op) {
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
            case ALIR_OP_EQ:
            case ALIR_OP_NEQ:
            case ALIR_OP_LT:
            case ALIR_OP_LTE:
            case ALIR_OP_GT:
            case ALIR_OP_GTE: {
                if (is_float) {
                    switch (inst->op) {
                        case ALIR_OP_EQ: res = LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, op1, op2, "eq_f"); break;
                        case ALIR_OP_NEQ: res = LLVMBuildFCmp(ctx->builder, LLVMRealONE, op1, op2, "neq_f"); break;
                        case ALIR_OP_LT: res = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, op1, op2, "lt_f"); break;
                        case ALIR_OP_LTE: res = LLVMBuildFCmp(ctx->builder, LLVMRealOLE, op1, op2, "lte_f"); break;
                        case ALIR_OP_GT: res = LLVMBuildFCmp(ctx->builder, LLVMRealOGT, op1, op2, "gt_f"); break;
                        case ALIR_OP_GTE: res = LLVMBuildFCmp(ctx->builder, LLVMRealOGE, op1, op2, "gte_f"); break;
                        default: break;
                    }
                } else {
                    switch (inst->op) {
                        case ALIR_OP_EQ: res = LLVMBuildICmp(ctx->builder, LLVMIntEQ, op1, op2, "eq"); break;
                        case ALIR_OP_NEQ: res = LLVMBuildICmp(ctx->builder, LLVMIntNE, op1, op2, "neq"); break;
                        case ALIR_OP_LT: res = LLVMBuildICmp(ctx->builder, LLVMIntSLT, op1, op2, "lt"); break;
                        case ALIR_OP_LTE: res = LLVMBuildICmp(ctx->builder, LLVMIntSLE, op1, op2, "lte"); break;
                        case ALIR_OP_GT: res = LLVMBuildICmp(ctx->builder, LLVMIntSGT, op1, op2, "gt"); break;
                        case ALIR_OP_GTE: res = LLVMBuildICmp(ctx->builder, LLVMIntSGE, op1, op2, "gte"); break;
                        default: break;
                    }
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
            case ALIR_OP_ROTL:
            case ALIR_OP_ROTR: {
                LLVMTypeRef ty = LLVMTypeOf(op1);
                unsigned bw = LLVMGetIntTypeWidth(ty);
                char name[64];
                if (inst->op == ALIR_OP_ROTL) {
                    snprintf(name, sizeof(name), "llvm.fshl.i%u", bw);
                } else {
                    snprintf(name, sizeof(name), "llvm.fshr.i%u", bw);
                }
                LLVMValueRef func = LLVMGetNamedFunction(ctx->llvm_mod, name);
                LLVMTypeRef args[] = { ty, ty, ty };
                LLVMTypeRef fty = LLVMFunctionType(ty, args, 3, 0);
                if (!func) {
                    func = LLVMAddFunction(ctx->llvm_mod, name, fty);
                }
                LLVMValueRef call_args[] = { op1, op1, op2 };
                res = LLVMBuildCall2(ctx->builder, fty, func, call_args, 3, "rot");
                break;
            }
            case ALIR_OP_BITCAST:
                res = LLVMBuildBitCast(ctx->builder, op1, get_llvm_type(ctx, inst->dest->type), "bitcast");
                break;
            case ALIR_OP_CAST: {
                LLVMTypeRef dst_ty = get_llvm_type(ctx, inst->dest->type);
                LLVMTypeRef src_ty = LLVMTypeOf(op1);
                LLVMTypeKind dst_kind = LLVMGetTypeKind(dst_ty);
                LLVMTypeKind src_kind = LLVMGetTypeKind(src_ty);
                if (src_kind == LLVMIntegerTypeKind && dst_kind == LLVMPointerTypeKind) {
                    res = LLVMBuildIntToPtr(ctx->builder, op1, dst_ty, "int2ptr");
                } else if (src_kind == LLVMPointerTypeKind && dst_kind == LLVMIntegerTypeKind) {
                    res = LLVMBuildPtrToInt(ctx->builder, op1, dst_ty, "ptr2int");
                } else if (src_kind == LLVMIntegerTypeKind && dst_kind == LLVMIntegerTypeKind) {
                    if (LLVMGetIntTypeWidth(dst_ty) > LLVMGetIntTypeWidth(src_ty)) {
                        res = inst->dest->type.is_unsigned ? LLVMBuildZExt(ctx->builder, op1, dst_ty, "zext") : LLVMBuildSExt(ctx->builder, op1, dst_ty, "sext");
                    } else if (LLVMGetIntTypeWidth(dst_ty) < LLVMGetIntTypeWidth(src_ty)) {
                        res = LLVMBuildTrunc(ctx->builder, op1, dst_ty, "trunc");
                    } else {
                        res = op1;
                    }
                } else if (src_kind == LLVMPointerTypeKind && dst_kind == LLVMStructTypeKind) {
                    res = LLVMBuildLoad2(ctx->builder, dst_ty, op1, "cast_load");
                } else if (src_kind == LLVMStructTypeKind && dst_kind == LLVMStructTypeKind) {
                    res = op1; // ALREADY SAME STRUCT
                } else if (src_kind == LLVMIntegerTypeKind && (dst_kind == LLVMDoubleTypeKind || dst_kind == LLVMFloatTypeKind)) {
                    res = inst->op1->type.is_unsigned ? LLVMBuildUIToFP(ctx->builder, op1, dst_ty, "ui2fp") : LLVMBuildSIToFP(ctx->builder, op1, dst_ty, "si2fp");
                } else if ((src_kind == LLVMDoubleTypeKind || src_kind == LLVMFloatTypeKind) && dst_kind == LLVMIntegerTypeKind) {
                    res = inst->dest->type.is_unsigned ? LLVMBuildFPToUI(ctx->builder, op1, dst_ty, "fp2ui") : LLVMBuildFPToSI(ctx->builder, op1, dst_ty, "fp2si");
                } else if ((src_kind == LLVMDoubleTypeKind || src_kind == LLVMFloatTypeKind) && (dst_kind == LLVMDoubleTypeKind || dst_kind == LLVMFloatTypeKind)) {
                    if (src_kind == LLVMDoubleTypeKind && dst_kind == LLVMFloatTypeKind) res = LLVMBuildFPTrunc(ctx->builder, op1, dst_ty, "fptrunc");
                    else if (src_kind == LLVMFloatTypeKind && dst_kind == LLVMDoubleTypeKind) res = LLVMBuildFPExt(ctx->builder, op1, dst_ty, "fpext");
                    else res = op1;
                } else {
                    res = LLVMBuildBitCast(ctx->builder, op1, dst_ty, "bitcast");
                }
                break;
            }
        default: break;
    }
    return res;
}
