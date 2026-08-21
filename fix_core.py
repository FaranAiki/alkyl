import re

with open('lib/wlroots/core.kyl', 'r') as f:
    text = f.read()

# 1. Replace classes with extern class
classes = ['wl_display', 'wlr_backend', 'wlr_renderer', 'wlr_allocator', 
           'wlr_compositor', 'wlr_data_device_manager', 'wlr_output_layout', 
           'wlr_xdg_shell', 'wlr_seat', 'wlr_scene', 'wlr_output', 
           'wlr_xdg_surface', 'wl_listener']

for cls in classes:
    text = re.sub(r'class\s+' + cls + r'\s*\{[^}]*\}', f'extern class {cls};', text)

# 2. Replace null* with null
text = text.replace('null*', 'null')

# 3. Replace if (VAR == null) with if (VAR == null ? true)
text = re.sub(r'if\s*\(\s*([a-zA-Z0-9_]+)\s*==\s*null\s*\)', r'if (\1 == null ? true)', text)

# 4. Add wlr_xdg_shell_create extern
text = text.replace(
    'errnum [ErrXdgShellCreation]', 
    '@c extern { wlroots.wlr_xdg_shell* wlr_xdg_shell_create(wlroots.wl_display* display, int version); }\n    errnum [ErrXdgShellCreation]'
)

# 5. Replace listeners logic
listener_replacements = {
    r'listener->notify\s*=\s*notify_func;': r'init_wl_listener(listener, notify_func);',
    r'void init_listener\(pristine wlroots.wl_listener\* listener, void\* notify_func\) \{\s*init_wl_listener\(listener, notify_func\);\s*\}': 
        r'void init_listener(pristine wlroots.wl_listener* listener, void* notify_func) {\n        init_wl_listener(listener, notify_func);\n    }',
    r'void backend_on_new_output\(pristine wlroots.wlr_backend\* backend, pristine wlroots.wl_listener\* listener, void\* notify_func\) \{\s*init_wl_listener\(listener, notify_func\);\s*wl_signal_add\(&\(backend->events.new_output\), listener\);\s*\}':
        r'void backend_on_new_output(pristine wlroots.wlr_backend* backend, pristine wlroots.wl_listener* listener, void* notify_func) {\n        wlr_backend_on_new_output(backend, listener, notify_func);\n    }',
    r'void output_on_frame\(pristine wlroots.wlr_output\* output, pristine wlroots.wl_listener\* listener, void\* notify_func\) \{\s*init_wl_listener\(listener, notify_func\);\s*wl_signal_add\(&\(output->events.frame\), listener\);\s*\}':
        r'void output_on_frame(pristine wlroots.wlr_output* output, pristine wlroots.wl_listener* listener, void* notify_func) {\n        wlr_output_on_frame(output, listener, notify_func);\n    }',
    r'void xdg_shell_on_new_surface\(pristine wlroots.wlr_xdg_shell\* shell, pristine wlroots.wl_listener\* listener, void\* notify_func\) \{\s*init_wl_listener\(listener, notify_func\);\s*wl_signal_add\(&\(shell->events.new_surface\), listener\);\s*\}':
        r'void xdg_shell_on_new_surface(pristine wlroots.wlr_xdg_shell* shell, pristine wlroots.wl_listener* listener, void* notify_func) {\n        wlr_xdg_shell_on_new_surface(shell, listener, notify_func);\n    }',
    r'void scene_add_xdg_surface\(pristine wlroots.wlr_scene\* scene, pristine wlroots.wlr_xdg_surface\* xdg_surface\) \{\s*if \(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL\) \{\s*wlr_scene_xdg_surface_create\(&\(scene->tree\), xdg_surface\);\s*wlr_xdg_toplevel_set_mapped\(xdg_surface->toplevel, true\);\s*\}\s*\}':
        r'void scene_add_xdg_surface(pristine wlroots.wlr_scene* scene, pristine wlroots.wlr_xdg_surface* xdg_surface) {\n        wlr_scene_add_xdg_surface(scene, xdg_surface);\n    }'
}

for k, v in listener_replacements.items():
    text = re.sub(k, v, text)

# Remove the extra `}` at the end if it exists.
# Wait, let's just make sure it balances.
braces = text.count('{') - text.count('}')
if braces < 0:
    # There are more `}` than `{`, let's remove the last `}`.
    last_brace_idx = text.rfind('}')
    text = text[:last_brace_idx] + text[last_brace_idx+1:]

with open('lib/wlroots/core.kyl', 'w') as f:
    f.write(text)

print(f'Done! Braces: {text.count("{") - text.count("}")}')
