import re

with open('src/parser/expr.c', 'r') as f:
    code = f.read()

# 1. Prevent LBRACKET from stealing array literal
new_code = code.replace("else if (p->current_token.type == TOKEN_LBRACKET) {", "else if (p->current_token.type == TOKEN_LBRACKET && (!p->current_token.has_space_before || p->in_space_separated_call > 0)) {")

# 2. Allow space separated call if lookahead has a comma, OR if function_call_require_comma is false
# wait, how to look ahead? Alkyl parser has peek() or lexer_peek? 
