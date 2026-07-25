import re

with open('src/lexer/lexer.c', 'r') as f:
    code = f.read()

new_code = code.replace("""  int prev_line = l->line;
  int is_first_token = (l->pos == 0);

  skip_whitespace_and_comments(l);""", """  int prev_line = l->line;
  int is_first_token = (l->pos == 0);

  int start_pos_before_skip = l->pos;
  skip_whitespace_and_comments(l);
  int has_space_before = (l->pos > start_pos_before_skip) || is_first_token;""")

new_code = new_code.replace("""  Token t = {TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col};""", """  Token t = {TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col, 0, has_space_before};""")

# Also in l->pending_tokens we don't care much, they are braces.
with open('src/lexer/lexer.c', 'w') as f:
    f.write(new_code)
