#include "vm_internal.h"
#include "semantic/semantic.h"
#include <string.h>

void vm_eval_misc(VMContext *ctx, AlirInst *inst) {
    switch(inst->op) {
case ALIR_OP_FALLBACK:
    break; // Nothing to do or not fully supported in REPL VM at compile time

case ALIR_OP_CAST:
case ALIR_OP_BITCAST: {
    if (inst->dest && inst->op1) {
        if (inst->op1->kind == ALIR_VAL_TEMP) {
            ctx->registers[inst->dest->temp_id] = ctx->registers[inst->op1->temp_id];
        } else if (inst->op1->kind == ALIR_VAL_CONST) {
            if (inst->op1->type.base == TYPE_SINGLE) ctx->registers[inst->dest->temp_id].as.single_val = inst->op1->val.single_val;
            else if (inst->op1->type.base == TYPE_DOUBLE) ctx->registers[inst->dest->temp_id].as.single_val = inst->op1->val.double_val;
            else ctx->registers[inst->dest->temp_id].as.int_val = inst->op1->val.long_long_val;
        } else if (inst->op1->kind == ALIR_VAL_VAR) {
            ctx->registers[inst->dest->temp_id].as.int_val = metalir_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
        } else if (inst->op1->kind == ALIR_VAL_GLOBAL && ctx->module) {
            long long ptr = metalir_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
            ctx->registers[inst->dest->temp_id].as.int_val = ptr;
        }
    }
    break;
}

default: break;
    }
}
