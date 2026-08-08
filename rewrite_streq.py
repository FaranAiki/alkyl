import os
import re

def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Find streq(a, "literal") and replace with streq_lit(a, "literal")
    # Also handles streq("literal", b) -> streq_lit(b, "literal")
    new_content = re.sub(r'\bstreq\s*\(\s*("[^"]*")\s*,\s*([^)]+)\s*\)', r'streq_lit(\2, \1)', content)
    new_content = re.sub(r'\bstreq\s*\(\s*([^,]+)\s*,\s*("[^"]*")\s*\)', r'streq_lit(\1, \2)', new_content)

    if new_content != content:
        with open(filepath, 'w') as f:
            f.write(new_content)
        print(f"Updated {filepath}")

for root, _, files in os.walk('src'):
    for file in files:
        if file.endswith('.c') or file.endswith('.h'):
            process_file(os.path.join(root, file))

for root, _, files in os.walk('include'):
    for file in files:
        if file.endswith('.c') or file.endswith('.h'):
            process_file(os.path.join(root, file))

