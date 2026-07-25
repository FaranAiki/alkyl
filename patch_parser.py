import re

with open('src/parser/expr.c', 'r') as f:
    code = f.read()

# 1. Modify LBRACKET check
code = code.replace("else if (p->current_token.type == TOKEN_LBRACKET) {", 
                    "else if (p->current_token.type == TOKEN_LBRACKET && (!p->current_token.has_space_before || p->in_space_separated_call > 0)) {")

# 2. Modify line 270
code = code.replace("else if (!p->settings.function_call_require_comma && p->in_space_separated_call == 0 && is_unambiguous_expr_start(p)) {",
                    "else if (p->in_space_separated_call == 0 && is_unambiguous_expr_start(p)) {")

with open('src/parser/expr.c', 'w') as f:
    f.write(code)

with open('src/semantic/table.c', 'r') as f:
    tcode = f.read()
    
# Remove size printing for arrays
# The code is:
#     if (t.array_size > 0) {
#         pos += snprintf(buf + pos, 256 - pos, "[%d]", t.array_size);
#         if (t.array_depth > 0) {
#             pos += snprintf(buf + pos, 256 - pos, "[%d]", t.array_depth);
#         }
#     }
tcode = tcode.replace('pos += snprintf(buf + pos, 256 - pos, "[%d]", t.array_size);', 'pos += snprintf(buf + pos, 256 - pos, "[]");')
tcode = tcode.replace('pos += snprintf(buf + pos, 256 - pos, "[%d]", t.array_depth);', 'pos += snprintf(buf + pos, 256 - pos, "[]");')

with open('src/semantic/table.c', 'w') as f:
    f.write(tcode)
