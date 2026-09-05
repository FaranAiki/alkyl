#define WLR_USE_UNSTABLE
#include <stdio.h>
#include <stddef.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_keyboard.h>

int main() {
    printf("xdg_surface.surface: %zu\n", offsetof(struct wlr_xdg_surface, surface));
    printf("xdg_surface.role: %zu\n", offsetof(struct wlr_xdg_surface, role));
    printf("xdg_shell.events.new_surface: %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
    printf("output.events.frame: %zu\n", offsetof(struct wlr_output, events.frame));
    printf("keyboard.events.key: %zu\n", offsetof(struct wlr_keyboard, events.key));
    printf("keyboard.modifiers: %zu\n", offsetof(struct wlr_keyboard, modifiers));
    return 0;
}
