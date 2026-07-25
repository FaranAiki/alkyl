import re

with open('src/parser/expr.c', 'r') as f:
    code = f.read()

type_literals = """  if (p->current_token.type == TOKEN_KW_INT || p->current_token.type == TOKEN_KW_LONG ||
      p->current_token.type == TOKEN_KW_CHAR || p->current_token.type == TOKEN_KW_FLOAT ||
      p->current_token.type == TOKEN_KW_DOUBLE || p->current_token.type == TOKEN_KW_VOID ||
      p->current_token.type == TOKEN_KW_BOOL || p->current_token.type == TOKEN_UNSIGNED ||
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

code = code.replace("if (p->current_token.type == TOKEN_TYPEOF) {", type_literals + "\n\n  if (p->current_token.type == TOKEN_TYPEOF) {")

with open('src/parser/expr.c', 'w') as f:
    f.write(code)

