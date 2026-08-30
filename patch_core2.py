import sys

with open('lib/wlroots/core.kyl', 'r') as f:
    text = f.read()

text = text.replace("extern class wlr_xdg_shell;", """
    class wlr_xdg_shell_events {
        wl_signal new_surface;
    }
    class wlr_xdg_shell {
        wlr_xdg_shell_events events;
    }
""")

text = text.replace("extern class wlr_xdg_surface;", """
    class wlr_xdg_surface {
        int role;
        void* surface; // actually wl_surface*
        wlr_xdg_toplevel* toplevel;
    }
""")

with open('lib/wlroots/core.kyl', 'w') as f:
    f.write(text)
