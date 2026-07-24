import sys

with open('src/meta/eval_mem.c', 'r') as f:
    content = f.read()

new_cases = """
case ALIR_OP_FREE_STACK:
    // Memory is tracked via arena in MetaVM, nothing to explicitly free
    break;
"""

if 'case ALIR_OP_FREE_STACK:' not in content:
    content = content.replace('default: break;', new_cases + '\ndefault: break;')
    with open('src/meta/eval_mem.c', 'w') as f:
        f.write(content)

