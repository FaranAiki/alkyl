with open('src/alir/generator.c', 'r') as f:
    content = f.read()

old_code = """        if (n->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)n;
            debug_alir("Visiting class %s\\n", cn->name);
            char *fqn = cn->name;"""
new_code = """        if (n->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)n;
            if (strstr(cn->name, "wl_listener")) {
                printf("DEBUG_REGISTER: wl_listener seen, has_body=%d\\n", cn->has_body);
            }
            debug_alir("Visiting class %s\\n", cn->name);
            char *fqn = cn->name;"""

if old_code in content:
    content = content.replace(old_code, new_code)
    with open('src/alir/generator.c', 'w') as f:
        f.write(content)
    print("Fixed fix12")
else:
    print("Could not find old_code")
