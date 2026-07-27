#include "vm_internal.h"
#include "semantic/semantic.h"
#include <string.h>

static long long resolve_src(AlirValue *op1, VMContext *ctx) {
    if (op1->kind == ALIR_VAL_TEMP) {
        return ctx->registers[op1->temp_id].as.int_val;
    } else if (op1->kind == ALIR_VAL_CONST) {
        if (op1->type.base == TYPE_SINGLE) {
            float f = op1->val.single_val;
            long long v; memcpy(&v, &f, sizeof(f));
            return v;
        } else if (op1->type.base == TYPE_DOUBLE) {
            double d = op1->val.double_val;
            long long v; memcpy(&v, &d, sizeof(d));
            return v;
        }
        return op1->val.long_long_val;
    }
    return metalir_vm_resolve_var(op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
}

void vm_eval_misc(VMContext *ctx, AlirInst *inst) {
    switch(inst->op) {
case ALIR_OP_FALLBACK:
    break;

case ALIR_OP_CAST:
case ALIR_OP_BITCAST: {
    if (inst->dest && inst->op1) {
        long long raw = resolve_src(inst->op1, ctx);
        int src_base = inst->op1->type.base;
        int dst_base = inst->dest->type.base;

        if (dst_base == TYPE_DOUBLE) {
            if (src_base == TYPE_DOUBLE) {
                memcpy(&ctx->registers[inst->dest->temp_id].as.single_val, &raw, sizeof(double));
            } else if (src_base == TYPE_SINGLE) {
                float f; memcpy(&f, &raw, sizeof(f));
                ctx->registers[inst->dest->temp_id].as.single_val = (double)f;
            } else {
                ctx->registers[inst->dest->temp_id].as.single_val = (double)raw;
            }
        } else if (dst_base == TYPE_SINGLE) {
            if (src_base == TYPE_DOUBLE) {
                double d; memcpy(&d, &raw, sizeof(d));
                ctx->registers[inst->dest->temp_id].as.single_val = (float)d;
            } else if (src_base == TYPE_SINGLE) {
                float f; memcpy(&f, &raw, sizeof(f));
                ctx->registers[inst->dest->temp_id].as.single_val = f;
            } else {
                ctx->registers[inst->dest->temp_id].as.single_val = (float)raw;
            }
        } else {
            if (src_base == TYPE_DOUBLE) {
                double d; memcpy(&d, &raw, sizeof(d));
                ctx->registers[inst->dest->temp_id].as.int_val = (long long)d;
            } else if (src_base == TYPE_SINGLE) {
                float f; memcpy(&f, &raw, sizeof(f));
                ctx->registers[inst->dest->temp_id].as.int_val = (long long)f;
            } else {
                ctx->registers[inst->dest->temp_id].as.int_val = raw;
            }
        }
    }
    break;
}

default: break;
    }
}
