#!/usr/bin/env python3
import re

files = [
    '/home/faranaiki/Git/alkyl/src/optlir/local/local_constprop.c',
    '/home/faranaiki/Git/alkyl/src/optlir/local/local_branch.c',
    '/home/faranaiki/Git/alkyl/src/optlir/local/local_blockset.c',
    '/home/faranaiki/Git/alkyl/src/optlir/local/local_unreach.c',
    '/home/faranaiki/Git/alkyl/src/optlir/local/local_merge.c',
    '/home/faranaiki/Git/alkyl/src/optlir/local/local_dead.c',
]

for fpath in files:
    with open(fpath, 'r') as f:
        content = f.read()
    
    content = re.sub(r'^static ([a-zA-Z_][a-zA-Z0-9_ \*]+\()', r'\1', content, flags=re.MULTILINE)
    
    with open(fpath, 'w') as f:
        f.write(content)
    print(f"Processed {fpath}")
