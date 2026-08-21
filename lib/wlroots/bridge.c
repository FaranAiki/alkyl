#include "bridge.h"
#include <stddef.h>

void* global_server_ptr = NULL;

extern void server_new_input(void* listener, void* data);
void* get_server_new_input_ptr() {
    return (void*)server_new_input;
}

void init_wl_listener(struct wl_listener *listener, void* notify) {
    listener->notify = notify;
}

void wlr_backend_on_new_output(struct wlr_backend *backend, struct wl_listener *listener, void* notify) {
    listener->notify = notify;
    wl_signal_add(&backend->events.new_output, listener);
}

void wlr_output_on_frame(struct wlr_output *output, struct wl_listener *listener, void* notify) {
    listener->notify = notify;
    wl_signal_add(&output->events.frame, listener);
}

void wlr_output_enable_and_commit(struct wlr_output *output, struct wlr_allocator *allocator, struct wlr_renderer *renderer) {
    wlr_output_init_render(output, allocator, renderer);
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
    if (mode != NULL) {
        wlr_output_state_set_mode(&state, mode);
    }
    wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);
}

void wlr_xdg_shell_on_new_surface(struct wlr_xdg_shell *shell, struct wl_listener *listener, void* notify) {
    listener->notify = notify;
    wl_signal_add(&shell->events.new_surface, listener);
}

void wlr_scene_add_xdg_surface(struct wlr_scene *scene, struct wlr_xdg_surface *xdg_surface) {
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        wlr_scene_xdg_surface_create(&scene->tree, xdg_surface);
        wlr_xdg_toplevel_set_mapped(xdg_surface->toplevel, true);
    }
}
