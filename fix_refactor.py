import os
import re

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

def refactor_ast_clone():
    with open('src/parser/ast_clone.c', 'r') as f:
        content = f.read()

    def process_switch(content, func_name_target, function_prefix):
        # find the function first
        func_idx = content.find(func_name_target)
        if func_idx == -1: return content, []

        switch_idx = content.find('switch (node->type) {', func_idx)
        if switch_idx == -1:
            switch_idx = content.find('switch(node->type)', func_idx)
        
        start_brace = content.find('{', switch_idx) + 1
        end_brace = find_matching_brace(content, start_brace)
        
        switch_body = content[start_brace:end_brace-1]
        
        cases = []
        idx = 0
        while True:
            case_idx = switch_body.find('case NODE_', idx)
            if case_idx == -1: break
                
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
                
            block_content = block_content.strip('\n')
            block_content = re.sub(r'\bbreak;\s*$', '', block_content)
                
            cases.append((case_name, block_content))
            idx = break_idx + 6

        new_functions = []
        new_cases = []
        
        for case_name, case_body in cases:
            func_name = f"{function_prefix}_{case_name.lower().replace('node_', '')}"
            
            if function_prefix == 'free':
                sig = f"static inline void {func_name}(ASTNode *node) {{"
                call = f"{func_name}(node);"
            else: # clone
                sig = f"static inline ASTNode* {func_name}(CompilerContext *ctx, ASTNode *node, char **type_params, VarType *replace_with, int num_params, char **rename_from, char **rename_to, int num_renames) {{"
                call = f"clone = {func_name}(ctx, node, type_params, replace_with, num_params, rename_from, rename_to, num_renames);"
            
            func_code = f"{sig}\n"
            if function_prefix == 'clone':
                func_code += f"    ASTNode *clone = NULL;\n"
                
            func_code += case_body.rstrip() + "\n"
            
            if function_prefix == 'clone':
                func_code += f"    return clone;\n"
                
            func_code += "}\n"
            new_functions.append(func_code)
            
            new_case = f"        case {case_name}: {{\n"
            new_case += f"            {call}\n"
            new_case += f"            break;\n"
            new_case += f"        }}"
            new_cases.append(new_case)
            
        new_switch_body = "\n".join(new_cases) + "\n        default: break;\n    "
        new_content = content[:switch_idx] + "switch (node->type) {\n" + new_switch_body + "}" + content[end_brace:]
        return new_content, new_functions

    content, clone_fns = process_switch(content, 'ASTNode* ast_clone(', 'clone')
    content, free_fns = process_switch(content, 'void free_ast(', 'free')
    
    insert_idx = content.rfind('#include')
    insert_idx = content.find('\n', insert_idx) + 1
    if insert_idx == -1: insert_idx = 0
    
    final_content = content[:insert_idx] + "\n\n" + "\n\n".join(clone_fns) + "\n\n" + "\n\n".join(free_fns) + "\n\n" + content[insert_idx:]

    with open('src/parser/ast_clone.c', 'w') as f:
        f.write(final_content)
        
def refactor_emitter():
    with open('src/parser/emitter.c', 'r') as f:
        content = f.read()

    func_idx = content.find('void parser_emit_node(')
    if func_idx == -1: return

    switch_idx = content.find('switch (node->type) {', func_idx)
    if switch_idx == -1:
        switch_idx = content.find('switch(node->type)', func_idx)
    
    start_brace = content.find('{', switch_idx) + 1
    end_brace = find_matching_brace(content, start_brace)
    
    switch_body = content[start_brace:end_brace-1]
    
    cases = []
    idx = 0
    while True:
        case_idx = switch_body.find('case NODE_', idx)
        if case_idx == -1: break
            
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
            
        block_content = block_content.strip('\n')
        block_content = re.sub(r'\bbreak;\s*$', '', block_content)
            
        cases.append((case_name, block_content))
        idx = break_idx + 6

    new_functions = []
    new_cases = []
    
    for case_name, case_body in cases:
        func_name = f"emit_{case_name.lower().replace('node_', '')}"
        
        sig = f"static inline void {func_name}(StringBuilder *sb, ASTNode *node, int indent) {{"
        call = f"{func_name}(sb, node, indent);"
        
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

    with open('src/parser/emitter.c', 'w') as f:
        f.write(final_content)

if __name__ == "__main__":
    os.system("git restore src/parser/ast_clone.c src/parser/emitter.c")
    refactor_ast_clone()
    refactor_emitter()
