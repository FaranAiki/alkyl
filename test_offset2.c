#include <stdio.h>
#include <stddef.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
int main() {
    printf("global %zu\n", offsetof(struct wlr_xdg_shell, global));
    printf("version %zu\n", offsetof(struct wlr_xdg_shell, version));
    printf("clients %zu\n", offsetof(struct wlr_xdg_shell, clients));
    printf("popup_grabs %zu\n", offsetof(struct wlr_xdg_shell, popup_grabs));
    printf("ping_timeout %zu\n", offsetof(struct wlr_xdg_shell, ping_timeout));
    printf("display_destroy %zu\n", offsetof(struct wlr_xdg_shell, display_destroy));
    printf("events.new_surface %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
    printf("events.new_toplevel %zu\n", offsetof(struct wlr_xdg_shell, events.new_toplevel));
    printf("events.new_popup %zu\n", offsetof(struct wlr_xdg_shell, events.new_popup));
    printf("events.destroy %zu\n", offsetof(struct wlr_xdg_shell, events.destroy));
    printf("data %zu\n", offsetof(struct wlr_xdg_shell, data));
    return 0;
}
