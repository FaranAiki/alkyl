import re

with open('lib/wlroots/core.kyl', 'r') as f:
    text = f.read()

extern_block = """    @c extern {
        void init_wl_listener(wl_listener* listener, void* notify);
        void wlr_backend_on_new_output(wlr_backend* backend, wl_listener* listener, void* notify);
        void wlr_output_on_frame(wlr_output* output, wl_listener* listener, void* notify);
        void wlr_output_enable_and_commit(wlr_output* output, wlr_allocator* allocator, wlr_renderer* renderer);
        void wlr_xdg_shell_on_new_surface(wlr_xdg_shell* shell, wl_listener* listener, void* notify);
        void wlr_scene_add_xdg_surface(wlr_scene* scene, wlr_xdg_surface* xdg_surface);
    }

"""

# Insert the extern block before `void init_listener`
text = text.replace('    void init_listener', extern_block + '    void init_listener')

# Also, there are calls to `wlr_output_state_*` that I forgot to replace with `wlr_output_enable_and_commit`!
# Ah! In my previous python script I did NOT replace the body of `void output_enable_and_commit(...)`!
# Let's replace the whole output_enable_and_commit function!
old_commit = """    void output_enable_and_commit(pristine wlroots.wlr_output* output, pristine wlroots.wlr_allocator* allocator, pristine wlroots.wlr_renderer* renderer) {
        wlr_output_init_render(output, allocator, renderer);
        wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, true);
        let mode = wlr_output_preferred_mode(output);
        if (mode != null) {
            wlr_output_state_set_mode(&state, mode);
        }
        wlr_output_commit_state(output, &state);
        wlr_output_state_finish(&state);
    }"""

new_commit = """    void output_enable_and_commit(pristine wlroots.wlr_output* output, pristine wlroots.wlr_allocator* allocator, pristine wlroots.wlr_renderer* renderer) {
        wlr_output_enable_and_commit(output, allocator, renderer);
    }"""

text = text.replace(old_commit, new_commit)

with open('lib/wlroots/core.kyl', 'w') as f:
    f.write(text)

