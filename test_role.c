#define WLR_USE_UNSTABLE
#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <stdio.h>
#include <stddef.h>
int main() {
    printf("role: %zu\n", offsetof(struct wlr_xdg_surface, role));
    return 0;
}
