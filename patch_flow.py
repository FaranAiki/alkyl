import sys

with open('src/meta/eval_flow.c', 'r') as f:
    content = f.read()

new_cases = """
case ALIR_OP_YIELD:
    // Yield not fully implemented in MetaVM, treat as a break/return for now
    break;
"""

if 'case ALIR_OP_YIELD:' not in content:
    content = content.replace('default: break;', new_cases + '\ndefault: break;')
    with open('src/meta/eval_flow.c', 'w') as f:
        f.write(content)

