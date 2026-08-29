#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("xdg_shell new_surface: %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
    return 0;
}
