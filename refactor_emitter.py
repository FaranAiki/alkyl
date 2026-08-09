import sys

def find_matching_brace(text, start_idx):
    count = 1
    i = start_idx
    while i < len(text) and count > 0:
        if text[i] == '{':
            count += 1
        elif text[i] == '}':
            count -= 1
        i += 1
    return i

def refactor_file(filename, function_prefix, first_arg_type, first_arg_name):
    with open(filename, 'r') as f:
        content = f.read()

    # Find the big switch (node->type)
    switch_idx = content.find('switch (node->type) {')
    if switch_idx == -1:
        print(f"Could not find switch in {filename}")
        return
        
    start_brace = content.find('{', switch_idx) + 1
    end_brace = find_matching_brace(content, start_brace)
    
    switch_body = content[start_brace:end_brace-1]
    
    cases = []
    
    idx = 0
    while True:
        case_idx = switch_body.find('case NODE_', idx)
        if case_idx == -1:
            break
            
        case_end_colon = switch_body.find(':', case_idx)
        case_name = switch_body[case_idx+5:case_end_colon].strip()
        
        block_start = switch_body.find('{', case_end_colon)
        break_idx = switch_body.find('break;', case_end_colon)
        
        if block_start != -1 and block_start < break_idx:
            block_end = find_matching_brace(switch_body, block_start + 1)
            break_idx = switch_body.find('break;', block_end)
            if break_idx == -1: break_idx = block_end
            block_content = switch_body[block_start+1:block_end-1]
        else:
            block_content = switch_body[case_end_colon+1:break_idx]
            
        cases.append((case_name, block_content.strip('\n')))
        idx = break_idx + 6

    new_functions = []
    new_cases = []
    
    for case_name, case_body in cases:
        func_name = f"{function_prefix}_{case_name.lower().replace('node_', '')}"
        
        if function_prefix == 'emit':
            sig = f"static inline void {func_name}(StringBuilder *sb, ASTNode *node, int indent) {{"
            call = f"{func_name}(sb, node, indent);"
        elif function_prefix == 'free':
            sig = f"static inline void {func_name}(ASTNode *node) {{"
            call = f"{func_name}(node);"
        
        func_code = f"{sig}\n"
        func_code += case_body.rstrip() + "\n"
        func_code += "}\n"
        new_functions.append(func_code)
        
        new_case = f"        case {case_name}: {{\n"
        new_case += f"            {call}\n"
        new_case += f"            break;\n"
        new_case += f"        }}"
        new_cases.append(new_case)
        
    new_switch_body = "\n".join(new_cases) + "\n        default: break;\n    "
    new_content = content[:switch_idx] + "switch (node->type) {\n" + new_switch_body + "}" + content[end_brace:]
    
    insert_idx = new_content.rfind('#include')
    insert_idx = new_content.find('\n', insert_idx) + 1
    if insert_idx == -1: insert_idx = 0
    
    final_content = new_content[:insert_idx] + "\n\n" + "\n\n".join(new_functions) + "\n\n" + new_content[insert_idx:]

    with open(filename, 'w') as f:
        f.write(final_content)
        
    print(f"{filename} refactored.")

refactor_file('src/parser/emitter.c', 'emit', 'StringBuilder *sb', 'sb')
refactor_file('src/parser/ast_clone.c', 'free', 'ASTNode *node', 'node')
