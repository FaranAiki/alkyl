import sys

# 1. Modify wmyl.kyl
with open('project/wmyl/wmyl.kyl', 'r') as f:
    wmyl_lines = f.readlines()

out_wmyl = []
in_wlroots_namespace = False

c_imports = []
wlroots_classes = []
wl_list_classes = []

for line in wmyl_lines:
    if '@c import' in line:
        c_imports.append(line)
        continue
    
    if 'export namespace wlroots {' in line:
        in_wlroots_namespace = True
        continue
        
    if in_wlroots_namespace:
        if '}' in line and not wlroots_classes[-1].startswith(' '):
            # End of namespace (assuming closing bracket is at col 0, wait, it's at col 0 in HEAD)
            pass
        if line.strip() == '}':
            in_wlroots_namespace = False
            continue
        if 'extern class wl_' in line or 'extern class wlr_' in line or 'extern class xcb_' in line or 'extern class wlr' in line or 'extern class Display' in line or 'extern class Backend' in line or 'extern class Renderer' in line or 'extern class Allocator' in line or 'extern class Scene' in line or 'extern class SceneTree' in line or 'extern class XdgShell' in line or 'extern class Cursor' in line or 'extern class XcursorManager' in line or 'extern class OutputLayout' in line or 'extern class Seat' in line or 'extern class ServerDecorationManager' in line or 'extern class XdgActivationV1' in line or 'extern class SessionLockManagerV1' in line or 'extern class DataDeviceManager' in line or 'extern class Compositor' in line or 'extern class Subcompositor' in line:
            wlroots_classes.append(line)
        continue
        
    out_wmyl.append(line)

# Now inject necessary C functions into wmyl.kyl
for i, line in enumerate(out_wmyl):
    if 'import "wlroots/core";' in line:
        out_wmyl.insert(i + 1, '\nextern int setenv(char* name, char* value, int overwrite);\nextern int fork();\nextern int execl(char* path, char* arg0, char* arg1, char* arg2, void* arg3);\nextern void _exit(int status);\nextern int clock_gettime(int clk_id, void* tp);\nextern void* malloc(long size);\nextern void free(void* ptr);\n\n')
        # Add wlroots namespace for the aliases used in class Backend
        out_wmyl.insert(i + 2, 'namespace wlroots {\n')
        for c in wlroots_classes:
            out_wmyl.insert(i + 3, c)
        out_wmyl.insert(i + len(wlroots_classes) + 3, '}\n')
        break

with open('project/wmyl/wmyl.kyl', 'w') as f:
    f.writelines(out_wmyl)


# 2. Modify core.kyl
with open('lib/wlroots/core.kyl', 'r') as f:
    core_lines = f.readlines()

out_core = []
# we need to put the C imports and extern classes inside export namespace wlroots
for i, line in enumerate(core_lines):
    out_core.append(line)
    if 'import "std/heap";' in line:
        out_core.extend(c_imports)
        out_core.append('\nexport namespace wlroots {\n')
        out_core.extend(wlroots_classes)
        out_core.append('}\n')

with open('lib/wlroots/core.kyl', 'w') as f:
    f.writelines(out_core)

