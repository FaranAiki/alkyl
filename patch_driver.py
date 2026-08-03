import re

with open('src/codegen_cranelift/driver.c', 'r') as f:
    code = f.read()

code = code.replace(
    'snprintf(cmd, sizeof(cmd), "gcc %s.o -o %s %s -lm", basename, basename, link_flags);',
    'snprintf(cmd, sizeof(cmd), "gcc %s.o src/runtime.c -o %s %s -lm", basename, basename, link_flags);'
)

with open('src/codegen_cranelift/driver.c', 'w') as f:
    f.write(code)
