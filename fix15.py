with open('src/alir/generator.c', 'r') as f:
    content = f.read()

old_code = """            if (st) { debug_alir("POPULATING %s has_body=%d\\n", fqn, cn->has_body); build_struct_fields(ctx, cn, st); } else { debug_alir("NOT FOUND %s\\n", fqn); }"""
new_code = """            if (st) { debug_alir("POPULATING %s has_body=%d\\n", fqn, cn->has_body); build_struct_fields(ctx, cn, st); } else { debug_alir("NOT FOUND %s\\n", fqn); }
            if (strstr(fqn, "wl_listener")) {
                printf("DEBUG_POPULATE: %s has_body=%d, st=%p\\n", fqn, cn->has_body, st);
                fflush(stdout);
            }"""

if old_code in content:
    content = content.replace(old_code, new_code)
    with open('src/alir/generator.c', 'w') as f:
        f.write(content)
    print("Fixed fix15")
else:
    print("Could not find old_code")
