/**
 * @file eval_misc.c
 * @brief Miscellaneous instruction evaluation for the Metalir VM.
 */
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

case ALIR_OP_SIZEOF: {
    if (inst->dest && inst->op1) {
        VarType t = inst->op1->type;
        debug_metalir("ALIR_OP_SIZEOF: base=%d, ptr_depth=%d, array_depth=%d, array_size=%d\n", 
            t.base, t.ptr_depth, t.array_depth, t.array_size);
        long sz = 0;
        if (t.is_tainted) {
            sz += 4; // Error code
        }
        
        long base_sz = 0;
        switch (t.base) {
            case TYPE_VOID: base_sz = 0; break;
            case TYPE_ERROR: base_sz = 4; break;
            case TYPE_INT: base_sz = 4; break;
            case TYPE_UNSIGNED_INT: base_sz = 4; break;
            case TYPE_SHORT: base_sz = 2; break;
            case TYPE_LONG: base_sz = 8; break;
            case TYPE_UNSIGNED_LONG: base_sz = 8; break;
            case TYPE_LONG_LONG: base_sz = 8; break;
            case TYPE_UNSIGNED_LONG_LONG: base_sz = 8; break;
            case TYPE_CHAR: base_sz = 1; break;
            case TYPE_UNSIGNED_CHAR: base_sz = 1; break;
            case TYPE_BOOL: base_sz = 1; break;
            case TYPE_SINGLE: base_sz = 4; break;
            case TYPE_DOUBLE: base_sz = 8; break;
            case TYPE_LONG_DOUBLE: base_sz = 8; break;
            case TYPE_CLASS: {
                if (t.class_name) {
                    // For JIT, class struct size is not easily available here.
                    // Let's fallback to 8 (pointer) as object refs are pointers.
                    // Wait, if it's a value type, we should look it up in sym table.
                    // We don't have symtable here easily. But wait!
                    // Let's just use 8 for now, as most classes are pointers.
                    base_sz = 8;
                } else {
                    base_sz = 8;
                }
                break;
            }
            case TYPE_ENUM: base_sz = 4; break;
            default: base_sz = 4; break;
        }

        if ((t.ptr_depth > 0 && t.array_size == 0) || t.is_func_ptr || (t.array_depth > 0 && t.array_size == 0)) {
            base_sz = 8;
        }

        if (t.array_size > 0) {
            base_sz = base_sz * t.array_size;
        }
        
        sz += base_sz;
        ctx->registers[inst->dest->temp_id].as.int_val = sz;
    }
    break;
}
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

case ALIR_OP_ALIGNOF: {
    if (inst->dest && inst->op1) {
        // Alignment is typically the same as sizeof for primitives, 8 for ptrs.
        ctx->registers[inst->dest->temp_id].as.int_val = 8; // simplified
    }
    break;
}

default: break;
    }
}
