with open('src/semantic/modifier/class.c', 'r') as f:
    content = f.read()

old_code = """            if (inner_cn->name && strncmp(inner_cn->name, "__anonymous_struct_", 19) == 0) {"""
new_code = """            if (inner_cn->name && (strncmp(inner_cn->name, "__anonymous_struct_", 19) == 0 || strncmp(inner_cn->name, "__anonymous_union", 17) == 0)) {"""

if old_code in content:
    content = content.replace(old_code, new_code)
    with open('src/semantic/modifier/class.c', 'w') as f:
        f.write(content)
    print("Fixed fix8")
else:
    print("Could not find old_code")
