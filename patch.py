import sys

content = """private namespace {
    extern void* malloc(long size);
    extern void free(void* ptr);
}

link "wayland-server";
link "wlroots-0.18";

export namespace wlroots {
    class wl_display {}
    class wlr_backend {}
    class wlr_renderer {}
    class wlr_allocator {}
    class wlr_compositor {}
    class wlr_data_device_manager {}
    class wlr_subcompositor {}
    class wlr_output_layout {}
    class wlr_xdg_shell {}
    class wlr_seat {}
    class wlr_output {}
    class wlr_xdg_surface {}
    class wlr_output_state {}
    class wlr_scene_tree {}
    class wlr_xdg_toplevel {}
    class wlr_scene {}

    @c import "wlr/backend.h"
    @c import "wlr/render/allocator.h"
    @c import "wlr/render/wlr_renderer.h"
    @c import "wlr/types/wlr_output.h"
    @c import "wlr/types/wlr_xdg_shell.h"
    @c import "wlr/types/wlr_scene.h"
    @c import "wlr/types/wlr_seat.h"
    @c import "wlr/types/wlr_compositor.h"
    @c import "wlr/types/wlr_data_device.h"
    @c import "wayland-server-core.h"

    extern wlr_output_layout* wlr_output_layout_create(wl_display* display);
    extern wlr_xdg_shell* wlr_xdg_shell_create(wl_display* display, int version);
    extern wlr_subcompositor* wlr_subcompositor_create(wl_display* display);
    extern void wlr_output_init_render(wlr_output* output, wlr_allocator* allocator, wlr_renderer* renderer);
    extern void wlr_output_state_init(wlr_output_state* state);
    extern void wlr_output_state_set_enabled(wlr_output_state* state, bool enabled);
    extern void wlr_output_state_set_mode(wlr_output_state* state, void* mode);
    extern void* wlr_output_preferred_mode(wlr_output* output);
    extern void wlr_output_commit_state(wlr_output* output, wlr_output_state* state);
    extern void wlr_output_state_finish(wlr_output_state* state);
    extern void wlr_scene_xdg_surface_create(wlr_scene_tree* tree, wlr_xdg_surface* xdg_surface);
"""
old_lines = open('lib/wlroots/core.kyl').readlines()
with open('lib/wlroots/core.kyl', 'w') as f:
    f.write(content)
    # find where extern wlr_output_layout_create starts in old_lines and write the rest
    start_idx = 0
    for i, line in enumerate(old_lines):
        if 'wlr_scene_xdg_surface_create' in line:
            start_idx = i + 1
            break
    for line in old_lines[start_idx:]:
        if '@c import' in line: continue
        if 'class ' in line and '{}' in line: continue
        f.write(line)
