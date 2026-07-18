import os, glob, re

files = glob.glob('src/parser/**/*.c', recursive=True)
files = [f for f in files if f not in ['src/parser/core.c', 'src/parser/ast_clone.c', 'src/parser/emitter.c']]

for f in files:
    with open(f, 'r') as file:
        lines = file.readlines()
        
    out = []
    in_func = False
    ret_stmt = "return NULL;"
    
    for i, line in enumerate(lines):
        out.append(line)
        
        m = re.match(r'^([A-Za-z_]+(?:\s*\*)*)\s*parse_[A-Za-z0-9_]+\s*\(', line)
        if m:
            in_func = True
            r = m.group(1).replace(' ', '')
            if 'ASTNode' in r or '*' in r:
                ret_stmt = "return NULL;"
            elif 'VarType' in r:
                ret_stmt = "return (VarType){0};"
            elif 'int' in r:
                ret_stmt = "return 0;"
            else:
                ret_stmt = "return;"
                
        # Handle cases where modifiers like `int parse_modifiers(Parser* p)`
        m2 = re.match(r'^int\s+parse_modifiers\(', line)
        if m2:
            in_func = True
            ret_stmt = "return 0;"
            
        if re.match(r'^}$', line):
            in_func = False
            
        if in_func:
            clean = re.sub(r'//.*', '', line)
            clean = re.sub(r'/\*.*?\*/', '', clean)
            
            # If line has a function call to eat or parse_
            if re.search(r'\b(?:eat|parse_[A-Za-z0-9_]+)\s*\(', clean):
                if clean.strip().endswith(';'):
                    if not re.match(r'^\s*return\b', clean):
                        if 'p->has_error' not in line and (i+1 >= len(lines) or 'p->has_error' not in lines[i+1]):
                            indent_len = len(line) - len(line.lstrip())
                            indent = " " * indent_len
                            # Insert right after
                            out.append(f"{indent}if (p->has_error) {ret_stmt}\n")

    with open(f, 'w') as file:
        file.writelines(out)

