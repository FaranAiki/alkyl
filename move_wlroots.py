import re

with open('project/wmyl/wmyl.kyl', 'r') as f:
    wmyl = f.read()

# Find export namespace wlroots
start = wmyl.find('export namespace wlroots {')
if start == -1:
    print("Not found")
    exit(1)

# Find matching brace
count = 0
end = -1
for i in range(start, len(wmyl)):
    if wmyl[i] == '{':
        count += 1
    elif wmyl[i] == '}':
        count -= 1
        if count == 0:
            end = i + 1
            break

namespace_block = wmyl[start:end]

# Extract @c import blocks
c_import_lines = []
new_wmyl_lines = []
for line in wmyl.split('\n'):
    if '@c import' in line:
        c_import_lines.append(line)
    else:
        new_wmyl_lines.append(line)

new_wmyl = '\n'.join(new_wmyl_lines)
new_wmyl = new_wmyl.replace(namespace_block, 'import "wlroots/core";\n')

# For core.kyl, we will replace the placeholder or just create it
core_content = """import "std/print";
import "std/heap";
import "std/vector";
import "std/string";

""" + '\n'.join(c_import_lines) + '\n\n' + namespace_block

with open('project/wmyl/wmyl.kyl', 'w') as f:
    f.write(new_wmyl)

with open('lib/wlroots/core.kyl', 'w') as f:
    f.write(core_content)

print("Moved successfully")
