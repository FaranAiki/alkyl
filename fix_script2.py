import re

with open('project/wmyl/wmyl.kyl', 'r') as f:
    content = f.read()

# Replace custom wl_signal_add with wlroots.wl_signal_add
content = re.sub(r'    @c void wl_signal_add\(wl_signal\* signal, wl_listener\* listener\) \{\n        wl_list_insert\(signal\.listener_list\.prev, \&\(listener\._link\)\);\n    \}\n', '', content)

# Change method signatures to use wlroots types
content = content.replace('wl_listener* listener', 'wlroots.wl_listener* listener')
content = content.replace('wlr_xdg_surface* xdg_surface', 'wlroots.wlr_xdg_surface* xdg_surface')
content = content.replace('wlr_keyboard* keyboard', 'wlroots.wlr_keyboard* keyboard')
content = content.replace('wlr_output* output', 'wlroots.wlr_output* output')
content = content.replace('wlr_xdg_shell* shell', 'wlroots.wlr_xdg_shell* shell')
content = content.replace('wl_display* display', 'wlroots.wl_display* display')

# Fix pointer arithmetic
content = re.sub(r'let signal = \(\(output as long\) \+ 168\) as wl_signal\*;', r'let signal = &(output.events.frame);', content)
content = re.sub(r'let signal = \(\(keyboard as long\) \+ 304\) as wl_signal\*;', r'let signal = &(keyboard.events.key);', content)
content = re.sub(r'let signal = \(\(shell.handle as long\) \+ 80\) as wl_signal\*;', r'let signal = &(shell.handle.events.new_surface);', content)
content = re.sub(r'let role_ptr = \(\(xdg_surface as long\) \+ 40\) as int\*;', r'let role_ptr = &(xdg_surface.role);', content)

# Replace wl_signal_add with wlroots.wl_signal_add
content = content.replace('wl_signal_add(signal, listener);', 'wlroots.wl_signal_add(signal, listener);')

with open('project/wmyl/wmyl.kyl', 'w') as f:
    f.write(content)
