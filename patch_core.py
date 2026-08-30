import sys

with open('lib/wlroots/core.kyl', 'r') as f:
    text = f.read()

text = text.replace("extern class wl_listener;", """
    class wl_listener {
        void* _link; // Actually wl_list but whatever
        void* notify;
        void* data;
    }
""")

with open('lib/wlroots/core.kyl', 'w') as f:
    f.write(text)
