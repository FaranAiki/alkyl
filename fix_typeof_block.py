import re

with open('src/parser/expr.c', 'r') as f:
    code = f.read()

typeof_block = """      if (is_type_start(p)) {
          VarType t = parse_type(p);
          while (p->current_token.type == TOKEN_LBRACKET) {
              eat(p, TOKEN_LBRACKET);
              if (p->current_token.type == TOKEN_RBRACKET) {
                  eat(p, TOKEN_RBRACKET);
                  t.ptr_depth++;
              } else {
                  while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
                      eat(p, p->current_token.type);
                  }
                  eat(p, TOKEN_RBRACKET);
                  t.array_size = 1; 
              }
          }
          // If typeof takes a type, we wrap it in a CastNode or a new TypeOfNode."""

code = code.replace("""      if (is_type_start(p)) {
          VarType t = parse_type(p);
          // If typeof takes a type, we wrap it in a CastNode or a new TypeOfNode.""", typeof_block)

with open('src/parser/expr.c', 'w') as f:
    f.write(code)

