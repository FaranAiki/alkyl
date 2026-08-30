import sys

with open('project/wmyl/wmyl.kyl', 'r') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    new_lines.append(line)
    if 'extern void free(void* ptr);' in line:
        new_lines.append("\nnamespace wlroots {\n")
        new_lines.append("    extern class Display;\n")
        new_lines.append("    extern class Backend;\n")
        new_lines.append("    extern class Renderer;\n")
        new_lines.append("    extern class Allocator;\n")
        new_lines.append("    extern class Compositor;\n")
        new_lines.append("    extern class OutputLayout;\n")
        new_lines.append("    extern class Scene;\n")
        new_lines.append("    extern class XdgShell;\n")
        new_lines.append("    extern class Seat;\n")
        new_lines.append("    extern class wl_listener;\n")
        new_lines.append("    extern class wlr_output;\n")
        new_lines.append("    extern class wlr_scene_output;\n")
        new_lines.append("    extern class wlr_xdg_surface;\n")
        new_lines.append("}\n")
        
with open('project/wmyl/wmyl.kyl', 'w') as f:
    f.writelines(new_lines)
