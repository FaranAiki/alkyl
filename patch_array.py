import re

with open('src/parser/c_parser.c', 'r') as f:
    lines = f.readlines()

new_lines = []
for i, line in enumerate(lines):
    if "while (c_match(p, C_TOKEN_LBRACKET)) {" in line:
        # Check if the previous lines declared a VarDeclNode*
        # We need to find the name of the variable.
        # It's either var, v, or it's just skipping.
        # Let's just define a temp_array_size.
        pass
    new_lines.append(line)
