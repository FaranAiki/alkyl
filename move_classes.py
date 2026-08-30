import sys

with open('lib/wlroots/core.kyl', 'r') as f:
    text = f.read()

classes = """
    class wlr_xdg_shell_events {
        wl_signal new_surface;
    }
    class wlr_xdg_shell {
        wlr_xdg_shell_events events;
    }
    class wlr_xdg_surface {
        int role;
        void* surface; // actually wl_surface*
        wlr_xdg_toplevel* toplevel;
    }
    class wl_listener {
        void* _link; // Actually wl_list but whatever
        void* notify;
        void* data;
    }
"""

text = text.replace(classes, "")
text = text.replace("}\n\n", "}\n" + classes + "\n")

with open('lib/wlroots/core.kyl', 'w') as f:
    f.write(text)
