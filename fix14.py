with open('src/alir/generator.c', 'r') as f:
    content = f.read()

old_code = """        if (n->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)n;
            if (strstr(cn->name, "wl_listener")) {
                printf("DEBUG_REGISTER: wl_listener seen, has_body=%d\\n", cn->has_body);
                ClassNode *tmp_ex = hashmap_get(&ctx->class_map, cn->name);
                if (tmp_ex) printf("  -> existing found! name=%s has_body=%d\\n", tmp_ex->name, tmp_ex->has_body);
                else printf("  -> NO existing found!\\n");
            }
            debug_alir("Visiting class %s\\n", cn->name);"""
new_code = """        if (n->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)n;
            if (strstr(cn->name, "wl_listener")) {
                debug_alir("DEBUG_REGISTER: wl_listener seen, has_body=%d\\n", cn->has_body);
                ClassNode *tmp_ex = hashmap_get(&ctx->class_map, cn->name);
                if (tmp_ex) debug_alir("  -> existing found! name=%s has_body=%d\\n", tmp_ex->name, tmp_ex->has_body);
                else debug_alir("  -> NO existing found!\\n");
            }
            debug_alir("Visiting class %s\\n", cn->name);"""

if old_code in content:
    content = content.replace(old_code, new_code)
    with open('src/alir/generator.c', 'w') as f:
        f.write(content)
    print("Fixed fix14")
else:
    print("Could not find old_code")
