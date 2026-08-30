#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("output.frame: %zu\n", offsetof(struct wlr_output, events.frame));
    printf("output.destroy: %zu\n", offsetof(struct wlr_output, events.destroy));
    printf("xdg_shell.new_surface: %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
    printf("xdg_surface.map: %zu\n", offsetof(struct wlr_xdg_surface, events.map));
    return 0;
}
