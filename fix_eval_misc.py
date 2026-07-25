import re

with open('src/meta/eval_misc.c', 'r') as f:
    code = f.read()

size_func = """static int get_type_size(VarType t) {
    int base_size = 8;
    if (t.ptr_depth > 0) {
        base_size = 8;
    } else {
        switch(t.base) {
            case TYPE_BOOL:
            case TYPE_CHAR:
            case TYPE_UNSIGNED_CHAR: base_size = 1; break;
            case TYPE_SHORT: base_size = 2; break;
            case TYPE_INT:
            case TYPE_UNSIGNED_INT:
            case TYPE_SINGLE: base_size = 4; break;
            case TYPE_LONG:
            case TYPE_UNSIGNED_LONG:
            case TYPE_LONG_LONG:
            case TYPE_UNSIGNED_LONG_LONG:
            case TYPE_DOUBLE: base_size = 8; break;
            default: base_size = 8; break;
        }
    }
    
    if (t.array_size > 0) {
        if (t.array_depth > 0) {
            return base_size * t.array_size * t.array_depth;
        }
        return base_size * t.array_size;
    }
    return base_size;
}
"""

replacement = """case ALIR_OP_SIZEOF: {
    if (inst->dest && inst->op1) {
        ctx->registers[inst->dest->temp_id].as.int_val = get_type_size(inst->op1->type);
    }
    break;
}
case ALIR_OP_ALIGNOF: {
    if (inst->dest && inst->op1) {
        int sz = get_type_size(inst->op1->type);
        if (inst->op1->type.array_size > 0) sz = get_type_size((VarType){ .base = inst->op1->type.base, .ptr_depth = inst->op1->type.ptr_depth });
        ctx->registers[inst->dest->temp_id].as.int_val = sz;
    }
    break;
}
case ALIR_OP_TYPEOF:"""

code = code.replace("void vm_eval_misc(VMContext *ctx, AlirInst *inst) {", size_func + "\nvoid vm_eval_misc(VMContext *ctx, AlirInst *inst) {")
code = code.replace("case ALIR_OP_SIZEOF:\ncase ALIR_OP_ALIGNOF:\ncase ALIR_OP_TYPEOF:", replacement)

with open('src/meta/eval_misc.c', 'w') as f:
    f.write(code)
