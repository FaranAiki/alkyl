with open('src/semantic/modifier/class.c', 'r') as f:
    content = f.read()

old_code = """            ClassNode *inner_cn = (ClassNode*)mem;
            if (inner_cn->name && strncmp(inner_cn->name, "__anonymous_struct_", 19) == 0) {"""
new_code = """            ClassNode *inner_cn = (ClassNode*)mem;
            printf("DEBUG: Hoisting nested ClassNode: %s\\n", inner_cn->name ? inner_cn->name : "NULL");
            if (inner_cn->name && strncmp(inner_cn->name, "__anonymous_struct_", 19) == 0) {"""

if old_code in content:
    content = content.replace(old_code, new_code)
    with open('src/semantic/modifier/class.c', 'w') as f:
        f.write(content)
    print("Fixed fix7")
else:
    print("Could not find old_code")
