import re
with open('src/parser/c_parser.c', 'r') as f:
    content = f.read()

def replacer(m):
    return """
    char buf_anon[64];
    snprintf(buf_anon, sizeof(buf_anon), "__anonymous_union_%%d", ++global_anon_counter);
    %s = arena_strdup(p->ctx->arena, buf_anon);
    """ % m.group(1)

content = re.sub(r'([a-zA-Z0-9_\.\->]+)\s*=\s*arena_strdup\(p->ctx->arena,\s*"__anonymous_union"\);', replacer, content)

with open('src/parser/c_parser.c', 'w') as f:
    f.write(content)
