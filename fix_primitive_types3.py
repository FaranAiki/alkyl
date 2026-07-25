import re

with open('src/parser/expr.c', 'r') as f:
    code = f.read()

# Fix the array_size assignment in the primitive type block
code = code.replace("""          if (p->current_token.type == TOKEN_RBRACKET) {
              eat(p, TOKEN_RBRACKET);
              t.ptr_depth++;""", """          if (p->current_token.type == TOKEN_RBRACKET) {
              eat(p, TOKEN_RBRACKET);
              t.array_size = 1;""")

with open('src/parser/expr.c', 'w') as f:
    f.write(code)

