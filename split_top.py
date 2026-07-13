import re
import os

with open("src/parser/top.c", "r") as f:
    stmt = f.read()

def extract_function(name, content):
    match = re.search(r'^(?:static\s+)?(?:ASTNode\*\s+|void\s+)?' + name + r'\s*\(.*?\)\s*\{', content, re.MULTILINE)
    if not match:
        return "", content
    
    start_idx = match.start()
    brace_count = 0
    in_str = False
    in_char = False
    in_comment = False
    in_line_comment = False
    
    i = match.end() - 1
    
    while i < len(content):
        c = content[i]
        
        if in_line_comment:
            if c == '\n':
                in_line_comment = False
        elif in_comment:
            if c == '*' and i + 1 < len(content) and content[i+1] == '/':
                in_comment = False
                i += 1
        elif in_str:
            if c == '\\':
                i += 1
            elif c == '"':
                in_str = False
        elif in_char:
            if c == '\\':
                i += 1
            elif c == "'":
                in_char = False
        else:
            if c == '/' and i + 1 < len(content):
                if content[i+1] == '/':
                    in_line_comment = True
                    i += 1
                elif content[i+1] == '*':
                    in_comment = True
                    i += 1
            elif c == '"':
                in_str = True
            elif c == "'":
                in_char = True
            elif c == '{':
                brace_count += 1
            elif c == '}':
                brace_count -= 1
                if brace_count == 0:
                    func_body = content[start_idx:i+1]
                    new_content = content[:start_idx] + content[i+1:]
                    return func_body, new_content
        i += 1
        
    return "", content

def extract_funcs(names, content):
    funcs = []
    for name in names:
        func, content = extract_function(name, content)
        if func:
            funcs.append(func)
    return "\n\n".join(funcs), content

class_funcs = ["parse_enum", "parse_class_impl", "parse_class"]

class_str, stmt = extract_funcs(class_funcs, stmt)

os.makedirs("src/parser/fragment", exist_ok=True)

header = '#include "../parser_internal.h"\n#include <string.h>\n#include <stdlib.h>\n#include <stdio.h>\n\n'

with open("src/parser/fragment/class.c", "w") as f:
    f.write(header + "static ASTNode* parse_class_impl(Parser *p, int modifiers);\n\n" + class_str)
with open("src/parser/top.c", "w") as f:
    stmt = re.sub(r'\n{3,}', '\n\n', stmt)
    f.write(stmt)

print("top.c split done.")
