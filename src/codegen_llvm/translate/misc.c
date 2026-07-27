#include "../../../include/codegen_llvm/translate.h"
#include "../../semantic/semantic.h"

LLVMValueRef translate_misc(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float) {
    (void)op1;
    (void)op2;
    (void)is_float;
    LLVMValueRef res = NULL;
    switch (inst->op) {
        default:
            break;
    }
    return res;
}
