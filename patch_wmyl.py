import sys

with open('project/wmyl/wmyl.kyl', 'r') as f:
    lines = f.readlines()

new_lines = []
skip = False
for line in lines:
    if '@c import' in line:
        continue
    if 'export namespace wlroots {' in line:
        skip = True
        continue
    if skip:
        # Wait, how many closing braces does the namespace have?
        if '}' in line and len(line.strip()) == 1 and line.startswith('}'):
            skip = False
        continue
    new_lines.append(line)

# Add standard library imports
for i, line in enumerate(new_lines):
    if 'import "wlroots/core";' in line:
        new_lines.insert(i + 1, '\nextern int setenv(char* name, char* value, int overwrite);\nextern int fork();\nextern int execl(char* path, char* arg0, char* arg1, char* arg2, void* arg3);\nextern void _exit(int status);\nextern int clock_gettime(int clk_id, void* tp);\nextern void* malloc(long size);\nextern void free(void* ptr);\n\n')
        break

with open('project/wmyl/wmyl.kyl', 'w') as f:
    f.writelines(new_lines)

