with open('src/codegen_llvm/codegen.c', 'r') as f:
    content = f.read()

old_code = """    // 1.5. Populate Struct Bodies
    st = ctx->alir_mod->structs;"""
new_code = """    // 1.5. Populate Struct Bodies
    st = ctx->alir_mod->structs;
    AlirStruct *tmp = st;
    while(tmp) {
        if (strcmp(tmp->name, "wlroots.wl_listener") == 0 || strcmp(tmp->name, "wl_listener") == 0) {
            printf("DEBUG_LLVM: struct %s has field_count %d\\n", tmp->name, tmp->field_count);
        }
        tmp = tmp->next;
    }"""

if old_code in content:
    content = content.replace(old_code, new_code)
    with open('src/codegen_llvm/codegen.c', 'w') as f:
        f.write(content)
    print("Fixed fix10")
else:
    print("Could not find old_code")
