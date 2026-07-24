import re

with open('src/meta/vm.c', 'r') as f:
    text = f.read()

start_idx = text.find('            switch (inst->op) {')
end_idx = text.find('            if (inst->op == ALIR_OP_PANIC) {')

if start_idx != -1 and end_idx != -1:
    new_switch = """
            VMContext ctx = {
                .vm = vm,
                .module = module,
                .func = func,
                .sem_ctx = sem_ctx,
                .args = args,
                .arg_count = arg_count,
                .registers = registers,
                .next_block = &next_block,
                .ret_val = &ret_val
            };

            switch (inst->op) {
                case ALIR_OP_ALLOCA:
                case ALIR_OP_STORE:
                case ALIR_OP_LOAD:
                case ALIR_OP_GET_PTR:
                    vm_eval_mem(&ctx, inst);
                    break;
                case ALIR_OP_ADD:
                case ALIR_OP_SUB:
                case ALIR_OP_MUL:
                case ALIR_OP_DIV:
                case ALIR_OP_MOD:
                case ALIR_OP_REM:
                case ALIR_OP_ROTL:
                case ALIR_OP_ROTR:
                case ALIR_OP_SHL:
                case ALIR_OP_SHR:
                case ALIR_OP_OR:
                case ALIR_OP_AND:
                case ALIR_OP_XOR:
                case ALIR_OP_EQ:
                case ALIR_OP_NEQ:
                case ALIR_OP_LT:
                case ALIR_OP_LE:
                case ALIR_OP_GT:
                case ALIR_OP_GE:
                case ALIR_OP_FDIV:
                    vm_eval_math(&ctx, inst);
                    break;
                case ALIR_OP_JUMP:
                case ALIR_OP_SWITCH:
                case ALIR_OP_CONDI:
                case ALIR_OP_RET:
                    vm_eval_flow(&ctx, inst);
                    break;
                case ALIR_OP_CALL:
                    vm_eval_call(&ctx, inst);
                    break;
                case ALIR_OP_DEFINED:
                    vm_eval_misc(&ctx, inst);
                    break;
                default: break;
            }
"""
    
    new_text = text[:start_idx] + new_switch + text[end_idx:]
    with open('src/meta/vm.c', 'w') as f:
        f.write(new_text)
    print("Replaced vm.c successfully.")
else:
    print("Could not find bounds.")
