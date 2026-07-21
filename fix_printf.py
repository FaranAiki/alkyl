import os, glob, re

for root, _, files in os.walk('test'):
    for f in files:
        if f.endswith('.aky') or f.endswith('.in'):
            path = os.path.join(root, f)
            with open(path, 'r') as file:
                content = file.read()
            if 'import "lib/c"' in content or "import \"lib/c\";" in content:
                # first fix the double clib from my sed
                content = content.replace('clib.clib.printf', 'clib.printf')
                
                # now replace printf with clib.printf
                # avoid replacing already prefixed ones
                content = re.sub(r'(?<!clib\.)\bprintf\b', 'clib.printf', content)
                
                with open(path, 'w') as file:
                    file.write(content)
