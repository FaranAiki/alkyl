import re
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

def refactor_file():
    with open('src/parser/ast_clone.c', 'r') as f:
        content = f.read()

    # Find `ASTNode* ast_clone(...)`
    # We will search for switch (node->type)
    switch_idx = content.find('switch (node->type) {')
    if switch_idx == -1:
        print("Could not find switch in ast_clone")
        return
        
    start_brace = content.find('{', switch_idx) + 1
    end_brace = find_matching_brace(content, start_brace)
    
    switch_body = content[start_brace:end_brace-1]
    
    # We will split cases by `case NODE_`
    cases = []
    
    idx = 0
    while True:
        case_idx = switch_body.find('case NODE_', idx)
        if case_idx == -1:
            break
            
        case_end_colon = switch_body.find(':', case_idx)
        case_name = switch_body[case_idx+5:case_end_colon].strip()
        
        # Now find the `{` for this case block
        block_start = switch_body.find('{', case_end_colon) + 1
        block_end = find_matching_brace(switch_body, block_start)
        
        # After block_end, there is a `break;`
        break_idx = switch_body.find('break;', block_end)
        
        block_content = switch_body[block_start:block_end-1]
        cases.append((case_name, block_content))
        
        idx = break_idx + 6

    new_functions = []
    new_cases = []
    
    for case_name, case_body in cases:
        func_name = f"clone_{case_name.lower().replace('node_', '')}"
        
        func_code = f"static inline ASTNode* {func_name}(CompilerContext *ctx, ASTNode *node, char **type_params, VarType *replace_with, int num_params, char **rename_from, char **rename_to, int num_renames) {{\n"
        func_code += f"    ASTNode *clone = NULL;\n"
        func_code += case_body.rstrip() + "\n"
        func_code += f"    return clone;\n"
        func_code += "}\n"
        new_functions.append(func_code)
        
        new_case = f"        case {case_name}: {{\n"
        new_case += f"            clone = {func_name}(ctx, node, type_params, replace_with, num_params, rename_from, rename_to, num_renames);\n"
        new_case += f"            break;\n"
        new_case += f"        }}"
        new_cases.append(new_case)

    # Let's also do the free_ast one? Wait, user said "each switch-statements".
    # I should write the new content back!
    
    new_switch_body = "\n".join(new_cases) + "\n        default: break;\n    "
    
    new_content = content[:switch_idx] + "switch (node->type) {\n" + new_switch_body + "}" + content[end_brace:]
    
    # insert functions before ast_clone
    insert_idx = new_content.rfind('ASTNode* ast_clone(')
    final_content = new_content[:insert_idx] + "\n\n".join(new_functions) + "\n\n" + new_content[insert_idx:]

    with open('src/parser/ast_clone.c', 'w') as f:
        f.write(final_content)
        
    print("ast_clone refactored.")

refactor_file()
