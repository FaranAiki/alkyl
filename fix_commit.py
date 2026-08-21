import re

with open('lib/wlroots/core.kyl', 'r') as f:
    text = f.read()

old_func = """    void output_enable_and_commit(pristine wlroots.wlr_output* output, pristine wlroots.wlr_allocator* allocator, pristine wlroots.wlr_renderer* renderer) {
        wlr_output_init_render(output, allocator, renderer);
        wlroots.wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, true);
        let mode = wlr_output_preferred_mode(output);
        if (mode != null) {
            wlr_output_state_set_mode(&state, mode);
        }
        wlr_output_commit_state(output, &state);
        wlr_output_state_finish(&state);
    }"""

new_func = """    void output_enable_and_commit(pristine wlroots.wlr_output* output, pristine wlroots.wlr_allocator* allocator, pristine wlroots.wlr_renderer* renderer) {
        wlr_output_enable_and_commit(output, allocator, renderer);
    }"""

text = text.replace(old_func, new_func)

with open('lib/wlroots/core.kyl', 'w') as f:
    f.write(text)

