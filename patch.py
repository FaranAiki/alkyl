import sys

with open('project/wmyl/wmyl.kyl', 'r') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    if '@c import' in line: continue
    new_lines.append(line)
    if 'import "wlroots/core";' in line:
        new_lines.extend([
            '\nextern int setenv(char* name, char* value, int overwrite);\n',
            'extern int fork();\n',
            'extern int execl(char* path, char* arg0, char* arg1, char* arg2, void* arg3);\n',
            'extern void _exit(int status);\n',
            'extern int clock_gettime(int clk_id, void* tp);\n',
            'extern void* malloc(long size);\n',
            'extern void free(void* ptr);\n',
            '\nnamespace wlroots {\n',
            '    extern class Display;\n',
            '    extern class Backend;\n',
            '    extern class Renderer;\n',
            '    extern class Allocator;\n',
            '    extern class Scene;\n',
            '    extern class SceneTree;\n',
            '    extern class XdgShell;\n',
            '    extern class Cursor;\n',
            '    extern class XcursorManager;\n',
            '    extern class OutputLayout;\n',
            '    extern class Seat;\n',
            '    extern class ServerDecorationManager;\n',
            '    extern class XdgActivationV1;\n',
            '    extern class SessionLockManagerV1;\n',
            '    extern class DataDeviceManager;\n',
            '    extern class Compositor;\n',
            '    extern class Subcompositor;\n',
            '    extern class wlr_output;\n',
            '    extern class wlr_scene_output;\n',
            '    extern class wl_listener;\n',
            '}\n\n'
        ])

with open('project/wmyl/wmyl.kyl', 'w') as f:
    f.writelines(new_lines)

with open('lib/wlroots/core.kyl', 'r') as f:
    lines = f.readlines()

new_lines = []
skip = False
for line in lines:
    if 'extern class wl_list;' in line: skip = True
    if skip and 'class wlr_xdg_shell_events {' in line: skip = False
    if skip: continue
    new_lines.append(line)

with open('lib/wlroots/core.kyl', 'w') as f:
    f.writelines(new_lines)
