import os
import re
import glob

def process_file(filepath):
    print(f"Processing {filepath}")
    with open(filepath, 'r') as f:
        content = f.read()

    # Find all functions starting with parse_
    # We'll use a regex to match the function signature and its body
    # This is a bit tricky with regex, so let's do it line by line
    
    lines = content.split('\n')
    new_lines = []
    
    current_return = None
    
    for i in range(len(lines)):
        line = lines[i]
        
        # Detect function signature
        # E.g. ASTNode* parse_something(...) {
        # VarType parse_something(...) {
        m = re.match(r'^([A-Za-z_]+(?:\s*\*)?)\s+parse_[A-Za-z0-9_]+\s*\(', line)
        if m:
            ret_type = m.group(1).replace(' ', '').strip()
            if 'ASTNode' in ret_type or '*' in ret_type:
                current_return = "return NULL;"
            elif 'VarType' in ret_type:
                current_return = "return (VarType){0};"
            elif 'int' in ret_type:
                current_return = "return 0;"
            else:
                current_return = "return;"
        
        new_lines.append(line)
        
        if current_return is not None:
            # Check if this line has eat(...) or parse_something(...)
            # But we only want to add it if it's a statement (ends with ;)
            # Or if it's inside a statement, wait for the semicolon.
            # A simple heuristic: if line contains "eat(" or "parse_" and ends with ";"
            # let's be careful about things like `ASTNode* node = parse_expr();`
            
            # Remove comments for checking
            clean_line = re.sub(r'//.*', '', line)
            clean_line = re.sub(r'/\*.*?\*/', '', clean_line)
            
            if re.search(r'\b(?:eat|parse_[A-Za-z0-9_]+)\s*\(', clean_line):
                # If there's already an if (p->has_error) on this line or the next, skip
                if 'p->has_error' in line:
                    continue
                if i + 1 < len(lines) and 'p->has_error' in lines[i+1]:
                    continue
                
                # Check if it's a complete statement (ends with ;)
                if clean_line.strip().endswith(';'):
                    # Check if it's inside a for loop or if statement condition...
                    # simple heuristic: if it starts with 'for' or 'if', we skip, unless the parse/eat is inside the block
                    # Actually, just check if the line starts with spaces and isn't part of a control structure condition without braces
                    
                    # Let's just insert it indented properly
                    indent = len(line) - len(line.lstrip())
                    new_lines.append(' ' * indent + f"if (p->has_error) {current_return}")
                    
    with open(filepath, 'w') as f:
        f.write('\n'.join(new_lines))

files = glob.glob('src/parser/**/*.c', recursive=True)
for f in files:
    if f.endswith('core.c') or f.endswith('ast_clone.c') or f.endswith('emitter.c'):
        continue
    process_file(f)

