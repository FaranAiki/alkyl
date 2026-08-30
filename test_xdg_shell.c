#define WLR_USE_UNSTABLE
#include <stdio.h>
#include <stddef.h>
#include <wlr/types/wlr_xdg_shell.h>
int main() {
    printf("xdg_shell.new_surface: %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
    printf("xdg_shell.destroy: %zu\n", offsetof(struct wlr_xdg_shell, events.destroy));
    return 0;
}
