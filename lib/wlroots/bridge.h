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
#include "xdg-shell-protocol.h"
#include <wlr/types/wlr_xdg_shell.h>

struct wlr_xdg_shell;
struct wlr_xdg_shell *wlr_xdg_shell_create(struct wl_display *display, unsigned int version);

typedef void (*AlkylNotifyFunc)(struct wl_listener *listener, void *data);

void init_wl_listener(struct wl_listener *listener, AlkylNotifyFunc notify);
void wlr_backend_on_new_output(struct wlr_backend *backend, struct wl_listener *listener, AlkylNotifyFunc notify);
void wlr_output_on_frame(struct wlr_output *output, struct wl_listener *listener, AlkylNotifyFunc notify);
void wlr_output_enable_and_commit(struct wlr_output *output, struct wlr_allocator *allocator, struct wlr_renderer *renderer);
void wlr_xdg_shell_on_new_surface(struct wlr_xdg_shell *shell, struct wl_listener *listener, AlkylNotifyFunc notify);
void wlr_scene_add_xdg_surface(struct wlr_scene *scene, struct wlr_xdg_surface *xdg_surface);
