import os

with open('project/wmyl/wmyl.kyl', 'r') as f:
    wmyl_lines = f.readlines()

core_kyl_lines = [
    'import "std/print";\n',
    'import "std/heap";\n',
    '\n',
    '@c import "wlr/backend.h"\n',
    'link "wayland-server";\n',
    'link "wlroots-0.18";\n',
    '@c import "wlr/render/allocator.h"\n',
    '@c import "wlr/render/wlr_renderer.h"\n',
    '@c import "wlr/types/wlr_output.h"\n',
    '@c import "wlr/types/wlr_xdg_shell.h"\n',
    '@c import "wlr/types/wlr_scene.h"\n',
    '@c import "wlr/types/wlr_seat.h"\n',
    '@c import "wlr/types/wlr_keyboard.h"\n',
    '@c import "wlr/types/wlr_compositor.h"\n',
    '@c import "wlr/types/wlr_data_device.h"\n',
    '@c import "wlr/types/wlr_pointer.h"\n',
    '@c import "wayland-server-core.h"\n',
    '\n',
    'extern void* malloc(long size);\n',
    'extern void free(void* ptr);\n',
    '\n'
]

# Find "export namespace wlroots {"
start_idx = -1
end_idx = -1
for i, line in enumerate(wmyl_lines):
    if line.startswith("export namespace wlroots {"):
        start_idx = i
        break

if start_idx != -1:
    for i in range(start_idx, len(wmyl_lines)):
        if wmyl_lines[i] == "}\n" and wmyl_lines[i+1] == "\n" and wmyl_lines[i+2] == "class Server {\n":
            end_idx = i
            break

if start_idx != -1 and end_idx != -1:
    core_kyl_lines.extend(wmyl_lines[start_idx:end_idx+1])
    
    with open('lib/wlroots/core.kyl', 'w') as f:
        f.writelines(core_kyl_lines)
    
    new_wmyl = wmyl_lines[:12] + ['import "wlroots/core";\n'] + wmyl_lines[26:start_idx-3] + wmyl_lines[end_idx+1:]
    # Fix the imports since lines 12-25 were the @c imports.
    with open('project/wmyl/wmyl.kyl', 'w') as f:
        f.writelines(new_wmyl)
    print("Refactoring done.")
else:
    print(f"Could not find bounds. start={start_idx}, end={end_idx}")

