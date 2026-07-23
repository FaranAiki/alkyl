#include "../../../include/codegen_llvm/translate.h"
#include "../../semantic/semantic.h"

LLVMValueRef translate_misc(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float) {
    LLVMValueRef res = NULL;
    switch (inst->op) {
        case ALIR_OP_SIZEOF: {
            LLVMTypeRef ty = get_llvm_type(ctx, inst->op1 ? inst->op1->type : inst->dest->type);
            if (inst->op1 && inst->op1->kind == ALIR_VAL_TYPE) {
                ty = get_llvm_type(ctx, inst->op1->type);
            }
            res = LLVMSizeOf(ty);
            break;
        }
        case ALIR_OP_ALIGNOF: {
            LLVMTypeRef ty = get_llvm_type(ctx, inst->op1 ? inst->op1->type : inst->dest->type);
            if (inst->op1 && inst->op1->kind == ALIR_VAL_TYPE) {
                ty = get_llvm_type(ctx, inst->op1->type);
            }
            res = LLVMAlignOf(ty);
            break;
        }
    }
    return res;
}
