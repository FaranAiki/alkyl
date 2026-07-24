import sys

with open('src/meta/eval_math.c', 'r') as f:
    content = f.read()

# Add ALIR_OP_NOT
if 'case ALIR_OP_NOT:' not in content:
    content = content.replace('case ALIR_OP_NEQ: {', 'case ALIR_OP_NOT:\n        case ALIR_OP_NEQ: {')

if 'else if (inst->op == ALIR_OP_NOT) res = ~v1;' not in content:
    content = content.replace('if (inst->op == ALIR_OP_ADD) res = v1 + v2;', 'if (inst->op == ALIR_OP_ADD) res = v1 + v2;\n                else if (inst->op == ALIR_OP_NOT) res = ~v1;')

# Replace FDIV case with FADD, FSUB, FMUL, FDIV
import re
fdiv_pattern = r'case ALIR_OP_FDIV: \{.*?(?=default: break;)'
new_fdiv = """case ALIR_OP_FADD:
        case ALIR_OP_FSUB:
        case ALIR_OP_FMUL:
        case ALIR_OP_FDIV: {
            if (inst->dest && inst->op1 && inst->op2) {
                double v1 = 0, v2 = 0;
                
                if (inst->op1->kind == ALIR_VAL_CONST) {
                    if (inst->op1->type.base == TYPE_SINGLE) { v1 = inst->op1->val.single_val; }
                    else { v1 = inst->op1->val.double_val; }
                } else if (inst->op1->kind == ALIR_VAL_TEMP) {
                    if (inst->op1->type.base == TYPE_SINGLE) { v1 = ctx->registers[inst->op1->temp_id].as.single_val; }
                    else { v1 = ctx->registers[inst->op1->temp_id].as.double_val; }
                }
                
                if (inst->op2->kind == ALIR_VAL_CONST) {
                    if (inst->op2->type.base == TYPE_SINGLE) { v2 = inst->op2->val.single_val; }
                    else { v2 = inst->op2->val.double_val; }
                } else if (inst->op2->kind == ALIR_VAL_TEMP) {
                    if (inst->op2->type.base == TYPE_SINGLE) { v2 = ctx->registers[inst->op2->temp_id].as.single_val; }
                    else { v2 = ctx->registers[inst->op2->temp_id].as.double_val; }
                }
                
                double res = 0;
                if (inst->op == ALIR_OP_FADD) res = v1 + v2;
                else if (inst->op == ALIR_OP_FSUB) res = v1 - v2;
                else if (inst->op == ALIR_OP_FMUL) res = v1 * v2;
                else if (inst->op == ALIR_OP_FDIV) res = v1 / v2;
                
                if (inst->dest->type.base == TYPE_SINGLE) {
                    ctx->registers[inst->dest->temp_id].as.single_val = res;
                } else {
                    ctx->registers[inst->dest->temp_id].as.double_val = res;
                }
            }
            break;
        }
        """
content = re.sub(fdiv_pattern, new_fdiv, content, flags=re.DOTALL)

with open('src/meta/eval_math.c', 'w') as f:
    f.write(content)

