import re

with open('src/meta/vm.c', 'r') as f:
    lines = f.readlines()

# find the switch start
start = 0
for i, line in enumerate(lines):
    if 'switch (inst->op) {' in line:
        start = i
        break

end = 0
for i in range(start, len(lines)):
    if '            if (inst->op == ALIR_OP_PANIC) {' in lines[i]:
        end = i
        break

switch_body = lines[start+1:end]

# Need to also extract the panic part (it's outside the switch)
panic_start = end
panic_end = 0
for i in range(panic_start, len(lines)):
    if '            inst = inst->next;' in lines[i]:
        panic_end = i
        break
panic_body = lines[panic_start:panic_end]

def extract_cases(body, op_list):
    res = []
    in_case = False
    brace_depth = 0
    for line in body:
        if not in_case:
            for op in op_list:
                if f'case {op}:' in line:
                    in_case = True
                    brace_depth = 0
                    break
        
        if in_case:
            res.append(line)
            brace_depth += line.count('{')
            brace_depth -= line.count('}')
            if brace_depth == 0 and 'break;' in line:
                in_case = False
                res.append('\n')
            elif brace_depth <= -1: # went out of bounds somehow, shouldn't happen for valid switch cases unless fallthrough
                pass # simplistic

    # Wait, the switch cases in vm.c are enclosed in braces:
    # case ALIR_OP_ADD: {
    #     ...
    #     break;
    # }
    # So brace_depth will go to 0 at the closing brace.
    
    return "".join(res)

print("Modularization script ready")
