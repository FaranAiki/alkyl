#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("C backend.events.new_output: %zu\n", offsetof(struct wlr_backend, events.new_output));
    printf("C backend.events.new_input: %zu\n", offsetof(struct wlr_backend, events.new_input));
    printf("C output.events.frame: %zu\n", offsetof(struct wlr_output, events.frame));
    printf("C shell.events.new_surface: %zu\n", offsetof(struct wlr_xdg_shell, events.new_surface));
    return 0;
}
