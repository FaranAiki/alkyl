import os

with open('src/meta/vm.c', 'r') as f:
    text = f.read()

def repl_ctx(code):
    import re
    code = re.sub(r'\bmodule\b', 'ctx->module', code)
    code = re.sub(r'\bvm\b', 'ctx->vm', code)
    code = re.sub(r'\bregisters\b', 'ctx->registers', code)
    code = re.sub(r'\bargs\b', 'ctx->args', code)
    code = re.sub(r'\barg_count\b', 'ctx->arg_count', code)
    code = re.sub(r'\bfunc\b', 'ctx->func', code)
    code = re.sub(r'\bsem_ctx\b', 'ctx->sem_ctx', code)
    code = re.sub(r'\bcurr_block\b', '(*ctx->curr_block)', code)
    code = re.sub(r'\bnext_block\b', '(*ctx->next_block)', code)
    code = re.sub(r'\bret_val\b', '(*ctx->ret_val)', code)
    return code

def extract_case(op):
    start_str = "case " + op + ":"
    start_idx = text.find(start_str)
    if start_idx == -1: return ""
    
    # Check if there is an immediate fallthrough (no { before next case)
    # Wait, the simplest way is to just find the brace and extract the block, including all preceding cases!
    # Let's find the brace.
    brace_idx = text.find("{", start_idx)
    if brace_idx == -1: return ""
    
    # backtrack to include all cases before this brace
    block_start = brace_idx
    while block_start > 0:
        if text[block_start-1] in ' \t\n':
            block_start -= 1
        elif text[block_start-5:block_start] == 'case ':
            # this is a case, we need to backtrack to before 'case '
            # wait, it's easier to just find the first case that has no break before it
            pass
            break
        else:
            break
            
    # Actually, let's just find the closing brace from brace_idx
    depth = 0
    in_str = False
    in_char = False
    for i in range(brace_idx, len(text)):
        if text[i] == '"' and text[i-1] != '\\': in_str = not in_str
        elif text[i] == "'" and text[i-1] != '\\': in_char = not in_char
        elif not in_str and not in_char:
            if text[i] == '{': depth += 1
            elif text[i] == '}': depth -= 1
        
        if depth == 0:
            end_idx = text.find("break;", i)
            if end_idx != -1 and end_idx < i + 20: 
                return text[start_idx:end_idx + 6]
            return text[start_idx:i + 1] + "\n                    break;"
    return ""

def extract_cases(ops):
    res = []
    for op in ops:
        c = extract_case(op)
        if c: res.append(c)
    return repl_ctx('\n'.join(res))

math_ops = ["ALIR_OP_ADD", "ALIR_OP_FDIV"] # Only extract one of each distinct block group
mem_ops = ["ALIR_OP_ALLOCA", "ALIR_OP_STORE", "ALIR_OP_LOAD", "ALIR_OP_GET_PTR"]
flow_ops = ["ALIR_OP_JUMP", "ALIR_OP_SWITCH", "ALIR_OP_CONDI", "ALIR_OP_RET"]
call_ops = ["ALIR_OP_CALL"]
misc_ops = ["ALIR_OP_DEFINED"]

with open('src/meta/eval_mem.c', 'w') as f:
    f.write('#include "meta/vm_internal.h"\n#include <string.h>\n#include "common/arena.h"\n\nvoid vm_eval_mem(VMContext *ctx, AlirInst *inst) {\n    switch(inst->op) {\n')
    f.write(extract_cases(mem_ops))
    f.write('\n        default: break;\n    }\n}\n')

with open('src/meta/eval_math.c', 'w') as f:
    f.write('#include "meta/vm_internal.h"\n#include "semantic/semantic.h"\n\nvoid vm_eval_math(VMContext *ctx, AlirInst *inst) {\n    long long v1 = 0, v2 = 0;\n    switch(inst->op) {\n')
    # Because we only extracted ADD, we need to explicitly write the other case labels so they fallthrough
    f.write('                case ALIR_OP_SUB:\n                case ALIR_OP_MUL:\n                case ALIR_OP_DIV:\n                case ALIR_OP_MOD:\n                case ALIR_OP_ROTL:\n                case ALIR_OP_ROTR:\n                case ALIR_OP_SHL:\n                case ALIR_OP_SHR:\n                case ALIR_OP_OR:\n                case ALIR_OP_AND:\n                case ALIR_OP_XOR:\n                case ALIR_OP_EQ:\n                case ALIR_OP_NEQ:\n                case ALIR_OP_LT:\n                case ALIR_OP_LTE:\n                case ALIR_OP_GT:\n                case ALIR_OP_GTE:\n')
    f.write(extract_cases(math_ops))
    f.write('\n        default: break;\n    }\n}\n')

with open('src/meta/eval_flow.c', 'w') as f:
    f.write('#include "meta/vm_internal.h"\n\nvoid vm_eval_flow(VMContext *ctx, AlirInst *inst) {\n    switch(inst->op) {\n')
    f.write(extract_cases(flow_ops))
    f.write('\n        default: break;\n    }\n}\n')

with open('src/meta/eval_call.c', 'w') as f:
    f.write('#include "meta/vm_internal.h"\n#include "common/arena.h"\n#include <string.h>\n#include <stdio.h>\n#ifdef HAVE_LIBFFI\n#include <ffi.h>\n#endif\n#ifndef _WIN32\n#include <dlfcn.h>\n#endif\n\nvoid vm_eval_call(VMContext *ctx, AlirInst *inst) {\n    switch(inst->op) {\n')
    f.write(extract_cases(call_ops))
    f.write('\n        default: break;\n    }\n}\n')

with open('src/meta/eval_misc.c', 'w') as f:
    f.write('#include "meta/vm_internal.h"\n#include "semantic/semantic.h"\n#include <string.h>\n\nvoid vm_eval_misc(VMContext *ctx, AlirInst *inst) {\n    switch(inst->op) {\n')
    f.write(extract_cases(misc_ops))
    f.write('\n        default: break;\n    }\n}\n')
