#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>
int main() {
    printf("output.events.frame offset = %zu\n", offsetof(struct wlr_output, events.frame));
    printf("xdg_shell.events.new_surface offset = %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
    printf("keyboard.events.key offset = %zu\n", offsetof(struct wlr_keyboard, events.key));
    return 0;
}
