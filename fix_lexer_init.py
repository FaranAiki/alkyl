import os
import re

for root, _, files in os.walk("src"):
    for file in files:
        if file.endswith(".c") or file.endswith(".h"):
            path = os.path.join(root, file)
            with open(path, "r") as f:
                content = f.read()
            
            # Find lexer_init calls
            # lexer_init(a, b, c, d) -> lexer_init(a, b, c, d, NULL)
            # except in cli.c it might be different, let's just do a regex replace
            new_content = re.sub(r'lexer_init\(([^,]+),\s*([^,]+),\s*([^,]+),\s*([^)]+)\)', r'lexer_init(\1, \2, \3, \4, NULL)', content)
            
            if new_content != content:
                with open(path, "w") as f:
                    f.write(new_content)
                    print(f"Updated {path}")
