import re

with open('src/semantic/check.c', 'r') as f:
    code = f.read()

code = code.replace("case NODE_TYPEOF:\n            sem_check_expr(ctx, node); \n            break;",
                    "case NODE_TYPEOF:\n        case NODE_SIZEOF:\n        case NODE_ALIGNOF:\n            sem_check_expr(ctx, node); \n            break;")

with open('src/semantic/check.c', 'w') as f:
    f.write(code)
