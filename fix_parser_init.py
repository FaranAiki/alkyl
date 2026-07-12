import os
import re

for root, _, files in os.walk("src"):
    for file in files:
        if file.endswith(".c") or file.endswith(".h"):
            path = os.path.join(root, file)
            with open(path, "r") as f:
                content = f.read()
            
            # Find parser_init calls
            # parser_init(a, b) -> parser_init(a, b, NULL)
            # except in cli.c which we already manually fixed
            if path == "src/driver/cli.c":
                continue
            
            new_content = re.sub(r'parser_init\(([^,]+),\s*([^)]+)\)', r'parser_init(\1, \2, NULL)', content)
            
            if new_content != content:
                with open(path, "w") as f:
                    f.write(new_content)
                    print(f"Updated {path}")
