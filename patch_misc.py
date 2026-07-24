import sys

with open('src/meta/eval_misc.c', 'r') as f:
    content = f.read()

new_cases = """
case ALIR_OP_SIZEOF:
case ALIR_OP_ALIGNOF:
case ALIR_OP_TYPEOF:
case ALIR_OP_FALLBACK:
case ALIR_OP_ITER_INIT:
case ALIR_OP_ITER_VALID:
case ALIR_OP_ITER_NEXT:
case ALIR_OP_ITER_GET:
    break; // Nothing to do or not fully supported in REPL VM at compile time

case ALIR_OP_MOV:
case ALIR_OP_CAST:
case ALIR_OP_BITCAST: {
    if (inst->dest && inst->op1) {
        if (inst->op1->kind == ALIR_VAL_TEMP) {
            ctx->registers[inst->dest->temp_id] = ctx->registers[inst->op1->temp_id];
        } else if (inst->op1->kind == ALIR_VAL_CONST) {
            if (inst->op1->type.base == TYPE_SINGLE) ctx->registers[inst->dest->temp_id].as.single_val = inst->op1->val.single_val;
            else if (inst->op1->type.base == TYPE_DOUBLE) ctx->registers[inst->dest->temp_id].as.double_val = inst->op1->val.double_val;
            else ctx->registers[inst->dest->temp_id].as.long_long_val = inst->op1->val.long_long_val;
        }
    }
    break;
}
"""

if 'case ALIR_OP_MOV:' not in content:
    content = content.replace('default: break;', new_cases + '\ndefault: break;')
    with open('src/meta/eval_misc.c', 'w') as f:
        f.write(content)

