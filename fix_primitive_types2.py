import re

with open('src/parser/expr.c', 'r') as f:
    code = f.read()

# Replace the previous primitive type block with one that correctly consumes []
old_block = """  if (p->current_token.type == TOKEN_KW_INT || p->current_token.type == TOKEN_KW_LONG ||
      p->current_token.type == TOKEN_KW_CHAR || p->current_token.type == TOKEN_KW_SINGLE ||
      p->current_token.type == TOKEN_KW_DOUBLE || p->current_token.type == TOKEN_KW_VOID ||
      p->current_token.type == TOKEN_KW_BOOL || p->current_token.type == TOKEN_KW_UNSIGNED ||
      p->current_token.type == TOKEN_KW_SHORT) {
      
      VarType t = parse_type(p);
      SizeOfNode *sn = parser_alloc(p, sizeof(SizeOfNode));
      sn->base.type = NODE_TYPEOF;
      sn->target_type = t;
      sn->operand = NULL;
      node = (ASTNode*)sn;
      set_loc(node, line, col);
      return node;
  }"""

new_block = """  if (p->current_token.type == TOKEN_KW_INT || p->current_token.type == TOKEN_KW_LONG ||
      p->current_token.type == TOKEN_KW_CHAR || p->current_token.type == TOKEN_KW_SINGLE ||
      p->current_token.type == TOKEN_KW_DOUBLE || p->current_token.type == TOKEN_KW_VOID ||
      p->current_token.type == TOKEN_KW_BOOL || p->current_token.type == TOKEN_KW_UNSIGNED ||
      p->current_token.type == TOKEN_KW_SHORT) {
      
      VarType t = parse_type(p);
      
      while (p->current_token.type == TOKEN_LBRACKET) {
          eat(p, TOKEN_LBRACKET);
          if (p->current_token.type == TOKEN_RBRACKET) {
              eat(p, TOKEN_RBRACKET);
              t.ptr_depth++;
          } else {
              // This is a fixed size array, skip expressions for now
              while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
                  eat(p, p->current_token.type);
              }
              eat(p, TOKEN_RBRACKET);
              t.array_size = 1; // Just a marker
          }
      }
      
      SizeOfNode *sn = parser_alloc(p, sizeof(SizeOfNode));
      sn->base.type = NODE_TYPEOF;
      sn->target_type = t;
      sn->operand = NULL;
      node = (ASTNode*)sn;
      set_loc(node, line, col);
      return node;
  }"""

if old_block in code:
    code = code.replace(old_block, new_block)
else:
    print("Warning: old_block not found!")

with open('src/parser/expr.c', 'w') as f:
    f.write(code)

