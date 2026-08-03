import re

with open('src/parser/core.c', 'r') as f:
    code = f.read()

code = code.replace(
    'paths[count++] = "/usr/share/alkyl";',
    'paths[count++] = "lib/";\n    paths[count++] = "/usr/share/alkyl";'
)

with open('src/parser/core.c', 'w') as f:
    f.write(code)
