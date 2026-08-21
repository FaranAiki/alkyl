#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

struct wl_list { void *prev; void *next; };
struct wl_listener { struct wl_list link; void *notify; };
struct wl_signal { struct wl_list listener_list; };
struct wl_global;

struct wlr_xdg_shell {
	struct wl_global *global;
	uint32_t version;
	struct wl_list clients;
	struct wl_list popup_grabs;
	uint32_t ping_timeout;

	struct wl_listener display_destroy;

	struct {
		struct wl_signal new_surface; // struct wlr_xdg_surface
		struct wl_signal new_toplevel; // struct wlr_xdg_toplevel
		struct wl_signal new_popup; // struct wlr_xdg_popup
		struct wl_signal destroy;
	} events;

	void *data;
};
int main() { printf("xdg_shell_events: %zu\n", offsetof(struct wlr_xdg_shell, events)); return 0; }
