import re

with open('project/wmyl/wmyl.kyl', 'r') as f:
    content = f.read()

# Remove manual wl_listener, wl_list, wl_signal, etc.
content = re.sub(r'    extern class wl_list;\n', '', content)
content = re.sub(r'    extern class wl_signal;\n', '', content)
content = re.sub(r'    class wl_listener \{\n        wl_list _link;\n        void\* notify;\n        void\* data;\n    \}\n', '', content)

# Remove other extern classes that are in C headers
content = re.sub(r'    extern class wlr_[a-zA-Z0-9_]+;\n', '', content)
content = re.sub(r'    extern class wl_[a-zA-Z0-9_]+;\n', '', content)

# Remove manual extern functions
content = re.sub(r'    extern void wl_display_terminate.*\n', '', content)
content = re.sub(r'    extern void wl_display_destroy.*\n', '', content)
content = re.sub(r'    extern void wlr_backend_.*\n', '', content)
content = re.sub(r'    extern wlr_.*_create.*\n', '', content)
content = re.sub(r'    extern void wlr_.*_init.*\n', '', content)
content = re.sub(r'    extern void wlr_.*_set_.*\n', '', content)
content = re.sub(r'    extern void\* wlr_output_preferred_mode.*\n', '', content)
content = re.sub(r'    extern void wlr_output_commit_state.*\n', '', content)
content = re.sub(r'    extern void wlr_output_state_finish.*\n', '', content)
content = re.sub(r'    extern void\* wlr_keyboard_from_input_device.*\n', '', content)
content = re.sub(r'    extern void wlr_scene_attach_output_layout.*\n', '', content)
content = re.sub(r'    extern void wlr_output_layout_add_auto.*\n', '', content)
content = re.sub(r'    extern void wl_list_insert.*\n', '', content)
content = re.sub(r'    extern void wlr_scene_xdg_surface_create.*\n', '', content)
content = re.sub(r'    extern void wlr_scene_output_commit.*\n', '', content)
content = re.sub(r'    extern void wlr_scene_output_send_frame_done.*\n', '', content)

with open('project/wmyl/wmyl.kyl', 'w') as f:
    f.write(content)
