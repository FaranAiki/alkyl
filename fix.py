import re

with open('src/meta/eval_math.c', 'r') as f:
    code = f.read()

# For ALIR_OP_EQ block, we need to extract double values if is_float.
new_code = code.replace("""
                if (inst->op1->kind == ALIR_VAL_CONST) {
                    if (inst->op1->type.base == TYPE_SINGLE) { v1 = inst->op1->val.single_val; }
                    else if (inst->op1->type.base == TYPE_DOUBLE) { v1 = inst->op1->val.double_val; }
                    else { v1 = inst->op1->val.long_long_val; }
                } else if (inst->op1->kind == ALIR_VAL_TEMP) {
                    if (inst->op1->type.base == TYPE_SINGLE) { v1 = ctx->registers[inst->op1->temp_id].as.single_val; }
                    else if (inst->op1->type.base == TYPE_DOUBLE) { v1 = ctx->registers[inst->op1->temp_id].as.double_val; }
                    else { v1 = ctx->registers[inst->op1->temp_id].as.int_val; }
                } else if (inst->op1->kind == ALIR_VAL_VAR) {
                    v1 = meta_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                } else {
                    if (inst->op1->type.base == TYPE_SINGLE || inst->op1->type.base == TYPE_DOUBLE) {
                        v1 = inst->op1->val.single_val;
                    } else {
                        v1 = inst->op1->val.long_long_val;
                    }
                }
                
                if (inst->op2->kind == ALIR_VAL_CONST) {
                    if (inst->op2->type.base == TYPE_SINGLE) { v2 = inst->op2->val.single_val; }
                    else if (inst->op2->type.base == TYPE_DOUBLE) { v2 = inst->op2->val.double_val; }
                    else { v2 = inst->op2->val.long_long_val; }
                } else if (inst->op2->kind == ALIR_VAL_TEMP) {
                    if (inst->op2->type.base == TYPE_SINGLE) { v2 = ctx->registers[inst->op2->temp_id].as.single_val; }
                    else if (inst->op2->type.base == TYPE_DOUBLE) { v2 = ctx->registers[inst->op2->temp_id].as.double_val; }
                    else { v2 = ctx->registers[inst->op2->temp_id].as.int_val; }
                } else if (inst->op2->kind == ALIR_VAL_VAR) {
                    v2 = meta_vm_resolve_var(inst->op2, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                } else {
                    if (inst->op2->type.base == TYPE_SINGLE || inst->op2->type.base == TYPE_DOUBLE) {
                        v2 = inst->op2->val.single_val;
                    } else {
                        v2 = inst->op2->val.long_long_val;
                    }
                }
""", """
                int is_float = 0;
                if (inst->op1->type.base == TYPE_SINGLE || inst->op1->type.base == TYPE_DOUBLE ||
                    inst->op2->type.base == TYPE_SINGLE || inst->op2->type.base == TYPE_DOUBLE) {
                    is_float = 1;
                }
                double f1 = 0, f2 = 0;

                if (inst->op1->kind == ALIR_VAL_CONST) {
                    if (inst->op1->type.base == TYPE_SINGLE) { v1 = inst->op1->val.single_val; f1 = v1; }
                    else if (inst->op1->type.base == TYPE_DOUBLE) { v1 = inst->op1->val.double_val; f1 = inst->op1->val.double_val; }
                    else { v1 = inst->op1->val.long_long_val; f1 = v1; }
                } else if (inst->op1->kind == ALIR_VAL_TEMP) {
                    if (inst->op1->type.base == TYPE_SINGLE || inst->op1->type.base == TYPE_DOUBLE) { f1 = ctx->registers[inst->op1->temp_id].as.single_val; v1 = (long long)f1; }
                    else { v1 = ctx->registers[inst->op1->temp_id].as.int_val; f1 = v1; }
                } else if (inst->op1->kind == ALIR_VAL_VAR) {
                    v1 = meta_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                    f1 = v1; // Variables resolving to float not fully supported this way, but fallback
                } else {
                    if (inst->op1->type.base == TYPE_SINGLE || inst->op1->type.base == TYPE_DOUBLE) {
                        f1 = inst->op1->val.single_val; v1 = (long long)f1;
                    } else {
                        v1 = inst->op1->val.long_long_val; f1 = v1;
                    }
                }
                
                if (inst->op2->kind == ALIR_VAL_CONST) {
                    if (inst->op2->type.base == TYPE_SINGLE) { v2 = inst->op2->val.single_val; f2 = v2; }
                    else if (inst->op2->type.base == TYPE_DOUBLE) { v2 = inst->op2->val.double_val; f2 = inst->op2->val.double_val; }
                    else { v2 = inst->op2->val.long_long_val; f2 = v2; }
                } else if (inst->op2->kind == ALIR_VAL_TEMP) {
                    if (inst->op2->type.base == TYPE_SINGLE || inst->op2->type.base == TYPE_DOUBLE) { f2 = ctx->registers[inst->op2->temp_id].as.single_val; v2 = (long long)f2; }
                    else { v2 = ctx->registers[inst->op2->temp_id].as.int_val; f2 = v2; }
                } else if (inst->op2->kind == ALIR_VAL_VAR) {
                    v2 = meta_vm_resolve_var(inst->op2, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                    f2 = v2;
                } else {
                    if (inst->op2->type.base == TYPE_SINGLE || inst->op2->type.base == TYPE_DOUBLE) {
                        f2 = inst->op2->val.single_val; v2 = (long long)f2;
                    } else {
                        v2 = inst->op2->val.long_long_val; f2 = v2;
                    }
                }
""")

new_code = new_code.replace("""                else if (inst->op == ALIR_OP_LT) res = v1 < v2;
                else if (inst->op == ALIR_OP_GT) res = v1 > v2;
                else if (inst->op == ALIR_OP_LTE) res = v1 <= v2;
                else if (inst->op == ALIR_OP_GTE) res = v1 >= v2;
                else if (inst->op == ALIR_OP_EQ) res = v1 == v2;
                else if (inst->op == ALIR_OP_NEQ) res = v1 != v2;""", """                else if (inst->op == ALIR_OP_LT) res = is_float ? (f1 < f2) : (v1 < v2);
                else if (inst->op == ALIR_OP_GT) res = is_float ? (f1 > f2) : (v1 > v2);
                else if (inst->op == ALIR_OP_LTE) res = is_float ? (f1 <= f2) : (v1 <= v2);
                else if (inst->op == ALIR_OP_GTE) res = is_float ? (f1 >= f2) : (v1 >= v2);
                else if (inst->op == ALIR_OP_EQ) res = is_float ? (f1 == f2) : (v1 == v2);
                else if (inst->op == ALIR_OP_NEQ) res = is_float ? (f1 != f2) : (v1 != v2);""")

# Remove my failed edit in ALIR_OP_FADD etc
new_code = new_code.replace(".as.double_val", ".as.single_val")

with open('src/meta/eval_math.c', 'w') as f:
    f.write(new_code)
