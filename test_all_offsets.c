#include <stdio.h>
#include <stddef.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
int main() {
    printf("wlr_xdg_shell.events.new_surface = %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
    printf("wlr_keyboard.events.key = %zu\n", offsetof(struct wlr_keyboard, events.key));
    printf("wlr_output.events.frame = %zu\n", offsetof(struct wlr_output, events.frame));
    return 0;
}
