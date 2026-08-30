#include <stdio.h>
#include <stddef.h>
#include <wlr/types/wlr_xdg_shell.h>
int main() {
    printf("new_surface: %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
    printf("role: %zu\n", offsetof(struct wlr_xdg_surface, role));
    printf("surface: %zu\n", offsetof(struct wlr_xdg_surface, surface));
    return 0;
}
