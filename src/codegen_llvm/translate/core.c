#include "../../../include/codegen_llvm/translate.h"
#include "../../semantic/semantic.h"

LLVMValueRef translate_stmt(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float);
LLVMValueRef translate_expr(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float);
LLVMValueRef translate_flow(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float);
LLVMValueRef translate_misc(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef op2, int is_float);

#include "../../include/codegen_llvm/translate.h"
#include "../semantic/semantic.h"

void translate_inst(CodegenCtx *ctx, AlirInst *inst) {
    LLVMValueRef op1 = get_llvm_value(ctx, inst->op1);
    LLVMValueRef op2 = get_llvm_value(ctx, inst->op2);
    LLVMValueRef res = NULL;

    int is_float = (inst->op1 && (inst->op1->type.base == TYPE_SINGLE || inst->op1->type.base == TYPE_DOUBLE));

    switch (inst->op) {
        case ALIR_OP_ALLOCA:
        case ALIR_OP_STORE:
        case ALIR_OP_LOAD:
        case ALIR_OP_GET_PTR:
            res = translate_stmt(ctx, inst, op1, op2, is_float);
            break;
            
        case ALIR_OP_ADD:
        case ALIR_OP_SUB:
        case ALIR_OP_MUL:
        case ALIR_OP_FADD:
        case ALIR_OP_FSUB:
        case ALIR_OP_FMUL:
        case ALIR_OP_DIV:
        case ALIR_OP_MOD:
        case ALIR_OP_FDIV:
        case ALIR_OP_AND:
        case ALIR_OP_OR:
        case ALIR_OP_XOR:
        case ALIR_OP_SHL:
        case ALIR_OP_SHR:
        case ALIR_OP_NOT:
        case ALIR_OP_EQ:
        case ALIR_OP_NEQ:
        case ALIR_OP_LT:
        case ALIR_OP_LTE:
        case ALIR_OP_GT:
        case ALIR_OP_GTE:
        case ALIR_OP_CAST:
            res = translate_expr(ctx, inst, op1, op2, is_float);
            break;
            
        case ALIR_OP_JUMP:
        case ALIR_OP_CONDI:
        case ALIR_OP_SWITCH:
        case ALIR_OP_PHI:
        case ALIR_OP_CALL:
        case ALIR_OP_RET:
            res = translate_flow(ctx, inst, op1, op2, is_float);
            break;
            
        default:
            res = translate_misc(ctx, inst, op1, op2, is_float);
            break;
    }
    if (inst->dest && res) {
        set_llvm_value(ctx, inst->dest, res);
    }

}
