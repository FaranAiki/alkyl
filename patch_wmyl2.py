import sys

with open('project/wmyl/wmyl.kyl', 'r') as f:
    text = f.read()

text = text.replace("""namespace wlroots {
    extern class Display;
    extern class Backend;
    extern class Renderer;
    extern class Allocator;
    extern class Compositor;
    extern class OutputLayout;
    extern class Scene;
    extern class XdgShell;
    extern class Seat;
    extern class wl_listener;
    extern class wlr_output;
    extern class wlr_scene_output;
    extern class wlr_xdg_surface;
}""", "")

with open('project/wmyl/wmyl.kyl', 'w') as f:
    f.write(text)
