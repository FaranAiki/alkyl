#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

struct wl_list { void *prev; void *next; };
struct wl_resource;
struct wlr_surface;
struct wlr_xdg_client;
struct wlr_xdg_toplevel;
struct wlr_xdg_popup;

enum wlr_xdg_surface_role {
	WLR_XDG_SURFACE_ROLE_NONE,
	WLR_XDG_SURFACE_ROLE_TOPLEVEL,
	WLR_XDG_SURFACE_ROLE_POPUP,
};

struct wlr_xdg_surface {
	struct wlr_xdg_client *client;
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wl_list link;

	enum wlr_xdg_surface_role role;
	struct wl_resource *role_resource;

	union {
		struct wlr_xdg_toplevel *toplevel;
		struct wlr_xdg_popup *popup;
	};
};

int main() {
    printf("role: %zu\n", offsetof(struct wlr_xdg_surface, role));
    printf("toplevel: %zu\n", offsetof(struct wlr_xdg_surface, toplevel));
    return 0;
}
