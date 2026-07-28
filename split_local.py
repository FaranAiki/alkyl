#!/usr/bin/env python3
import re
import os

src_path = '/home/faranaiki/Git/alkyl/src/optlir/local.c'
local_dir = '/home/faranaiki/Git/alkyl/src/optlir/local'
os.makedirs(local_dir, exist_ok=True)

with open(src_path) as f:
    content = f.read()

# Remove 'static ' from function definitions
content = re.sub(r'^static ([a-zA-Z_][a-zA-Z0-9_ \*]+\()', r'\1', content, flags=re.MULTILINE)

# Find all function definitions
i = 0
functions = []
while i < len(content):
    if content[i] == '#':
        while i < len(content) and content[i] != '\n': i += 1
        continue
    if content[i] == '/' and i+1 < len(content) and content[i+1] == '/':
        while i < len(content) and content[i] != '\n': i += 1
        continue
    if content[i] == '/' and i+1 < len(content) and content[i+1] == '*':
        i += 2
        while i < len(content) and not (content[i] == '*' and i+1 < len(content) and content[i+1] == '/'):
            i += 1
        i += 2
        continue
    match = re.match(r'^([a-zA-Z_][a-zA-Z0-9_ \*]+?)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(', content[i:])
    if match:
        func_name = match.group(2)
        j = i + match.end()
        while j < len(content) and content[j] != '{' and content[j] != ';': j += 1
        if j < len(content) and content[j] == '{':
            brace = 1
            j += 1
            while j < len(content) and brace > 0:
                if content[j] == '{': brace += 1
                elif content[j] == '}': brace -= 1
                j += 1
            functions.append((func_name, content[i:j]))
            i = j
            continue
        else:
            i = j + 1
            continue
    i += 1

# Group functions into logical files
groups = {
    'local_eval': [name for name, _ in functions if 'eval' in name or 'const' in name or name in ['is_identity_op', 'is_self_cancel_op', 'is_temp_used_except_in_load', 'all_args_const']],
    'local_constprop': [name for name, _ in functions if 'propagate' in name or 'param' in name or 'pure' in name],
    'local_branch': [name for name, _ in functions if 'branch' in name or 'fold' in name],
    'local_blockset': [name for name, _ in functions if 'block_set' in name or 'block' in name],
    'local_unreach': [name for name, _ in functions if 'reachable' in name or 'unreachable' in name],
    'local_merge': [name for name, _ in functions if 'merge' in name or 'redirect' in name],
    'local_dead': [name for name, _ in functions if 'dead' in name or 'used' in name or 'instruction' in name],
    'local_main': [name for name, _ in functions if 'optlir_local_optimize' in name],
}

# Write files
func_map = {name: body for name, body in functions}
for group_name, func_names in groups.items():
    bodies = []
    for name in func_names:
        if name in func_map:
            bodies.append(func_map[name])
    if bodies:
        code = '#include "optlir.h"\n#include "optlir/local.h"\n#include <stdlib.h>\n#include <string.h>\n#include <stdio.h>\n\n' + '\n\n'.join(bodies)
        with open(f'{local_dir}/{group_name}.c', 'w') as f:
            f.write(code)
        print(f'Created {group_name}.c')

# Remove old local.c
os.remove(src_path)
print('Removed old local.c')
