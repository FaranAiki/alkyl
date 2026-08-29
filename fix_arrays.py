import re

with open('src/parser/c_parser.c', 'r') as f:
    code = f.read()

# Replace:
# while (c_match(p, C_TOKEN_LBRACKET)) {
#     c_eat(p, C_TOKEN_LBRACKET);
#     int depth = 1;
#
# With:
# while (c_match(p, C_TOKEN_LBRACKET)) {
#     c_eat(p, C_TOKEN_LBRACKET);
#     if (c_match(p, C_TOKEN_NUMBER)) {
#         array_size = atoi(p->current.text);
#         c_eat(p, C_TOKEN_NUMBER);
#     }
#     int depth = 1;

def repl(m):
    indent = m.group(1)
    return f"{indent}while (c_match(p, C_TOKEN_LBRACKET)) {{\n{indent}    c_eat(p, C_TOKEN_LBRACKET);\n{indent}    if (c_match(p, C_TOKEN_NUMBER)) {{\n{indent}        array_size = atoi(p->current.text);\n{indent}        c_eat(p, C_TOKEN_NUMBER);\n{indent}    }}\n{indent}    int depth = 1;"

code = re.sub(r'(\s+)while \(c_match\(p, C_TOKEN_LBRACKET\)\) \{\n\s+c_eat\(p, C_TOKEN_LBRACKET\);\n\s+int depth = 1;', repl, code)

with open('src/parser/c_parser.c', 'w') as f:
    f.write(code)
