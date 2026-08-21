/**
 * @file misc.c
 * @brief LLVM miscellaneous translation implementation.
 */
#include "../../../include/codegen_llvm/translate.h"
#include "../../semantic/semantic.h"

/**
 * @brief Translate a miscellaneous ALIR instruction to LLVM IR.
 * @param ctx Code generation context.
 * @param inst ALIR instruction to translate.
 * @param op1 Pre-resolved LLVM value for operand 1.
 * @param op2 Pre-resolved LLVM value for operand 2.
 * @param is_float Non-zero if the operation involves floating-point operands.
 * @return Generated LLVM value, or NULL on failure.
 */
LLVMValueRef translate_misc(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float) {
    (void)ctx;
    (void)op1;
    (void)op2;
    (void)is_float;
    LLVMValueRef res = NULL;
    switch (inst->op) {
        case ALIR_OP_SIZEOF: {
            LLVMTypeRef ty = get_llvm_type(ctx, inst->op1->type);
            res = LLVMSizeOf(ty);
            break;
        }
        case ALIR_OP_ALIGNOF: {
            LLVMTypeRef ty = get_llvm_type(ctx, inst->op1->type);
            res = LLVMAlignOf(ty);
            break;
        }
        default:
            break;
    }
    return res;
}
