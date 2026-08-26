with open('src/parser/c_parser.c', 'r') as f:
    content = f.read()

old_code = """                    anon->name = arena_strdup(p->ctx->arena, inner_type.class_name ? inner_type.class_name : "__anonymous_struct");"""
new_code = """                    static int anon_counter = 0;
                    if (inner_type.class_name) {
                        anon->name = arena_strdup(p->ctx->arena, inner_type.class_name);
                    } else {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "__anonymous_struct_%d", ++anon_counter);
                        anon->name = arena_strdup(p->ctx->arena, buf);
                    }"""

if old_code in content:
    content = content.replace(old_code, new_code)
    print("Fixed anonymous struct name clash")
else:
    print("Could not find old_code")

with open('src/parser/c_parser.c', 'w') as f:
    f.write(content)
