#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("wlr_output.events.frame: %zu\n", offsetof(struct wlr_output, events.frame));
    printf("wlr_xdg_shell.events.new_surface: %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
}
