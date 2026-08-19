#define WLR_USE_UNSTABLE 1
#include <pixman.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
struct wlr_xdg_shell;
struct wlr_xdg_shell *wlr_xdg_shell_create(struct wl_display *display, unsigned int version);

typedef void (*AlkylNotifyFunc)(struct wl_listener *listener, void *data);
static inline void init_wl_listener(struct wl_listener *listener, AlkylNotifyFunc notify) {
    listener->notify = notify;
}
static inline void wlr_backend_on_new_output(struct wlr_backend *backend, struct wl_listener *listener, AlkylNotifyFunc notify) {
    listener->notify = notify;
    wl_signal_add(&backend->events.new_output, listener);
}
static inline void wlr_output_on_frame(struct wlr_output *output, struct wl_listener *listener, AlkylNotifyFunc notify) {
    listener->notify = notify;
    wl_signal_add(&output->events.frame, listener);
}
static inline void wlr_output_enable_and_commit(struct wlr_output *output, struct wlr_allocator *allocator, struct wlr_renderer *renderer) {
    wlr_output_init_render(output, allocator, renderer);
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
    if (mode) wlr_output_state_set_mode(&state, mode);
    wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);
}
#include "xdg-shell-protocol.h"
#include <wlr/types/wlr_xdg_shell.h>
static inline void wlr_xdg_shell_on_new_surface(struct wlr_xdg_shell *shell, struct wl_listener *listener, AlkylNotifyFunc notify) {
    listener->notify = notify;
    wl_signal_add(&shell->events.new_surface, listener);
}
static inline void wlr_scene_add_xdg_surface(struct wlr_scene *scene, struct wlr_xdg_surface *xdg_surface) {
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        wlr_scene_xdg_surface_create(&scene->tree, xdg_surface);
        wlr_xdg_toplevel_set_mapped(xdg_surface->toplevel, true);
    }
}
