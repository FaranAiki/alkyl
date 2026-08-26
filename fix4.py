import re
with open('src/parser/c_parser.c', 'r') as f:
    content = f.read()

if "int global_anon_counter = 0;" not in content:
    content = content.replace('#include "parser_internal.h"', '#include "parser_internal.h"\nint global_anon_counter = 0;')

def replacer(m):
    return """
    char buf_anon[64];
    snprintf(buf_anon, sizeof(buf_anon), "__anonymous_struct_%%d", ++global_anon_counter);
    %s = arena_strdup(p->ctx->arena, buf_anon);
    """ % m.group(1)

content = re.sub(r'([a-zA-Z0-9_\.\->]+)\s*=\s*arena_strdup\(p->ctx->arena,\s*"__anonymous_struct"\);', replacer, content)

def replacer2(m):
    return """
    if (%s) {
        %s = arena_strdup(p->ctx->arena, %s);
    } else {
        char buf_anon[64];
        snprintf(buf_anon, sizeof(buf_anon), "__anonymous_struct_%%d", ++global_anon_counter);
        %s = arena_strdup(p->ctx->arena, buf_anon);
    }
    """ % (m.group(2), m.group(1), m.group(2), m.group(1))

content = re.sub(r'([a-zA-Z0-9_\.\->]+)\s*=\s*arena_strdup\(p->ctx->arena,\s*([a-zA-Z0-9_\.\->]+)\s*\?\s*\2\s*:\s*"__anonymous_struct"\);', replacer2, content)

with open('src/parser/c_parser.c', 'w') as f:
    f.write(content)
