import os

replacements = {
    "NODE_ARRAY_ACCESS": "NODE_INDEX_ACCESS",
    "ArrayAccessNode": "IndexAccessNode",
    "sem_check_array_access": "sem_check_index_access",
    "alir_gen_expr_array_access": "alir_gen_expr_index_access",
    "alir_gen_addr_array_access": "alir_gen_addr_index_access",
    "array_access": "index_access"
}

def replace_in_file(filepath):
    if not filepath.endswith(('.c', '.h', '.aky')):
        return
    with open(filepath, 'r') as f:
        content = f.read()
        
    original = content
    for old, new in replacements.items():
        content = content.replace(old, new)
        
    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"Updated {filepath}")

for root, dirs, files in os.walk('src'):
    for file in files:
        replace_in_file(os.path.join(root, file))

for root, dirs, files in os.walk('include'):
    for file in files:
        replace_in_file(os.path.join(root, file))

