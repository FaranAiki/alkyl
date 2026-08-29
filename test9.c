#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("backend new_input: %zu\n", offsetof(struct wlr_backend, events.new_input));
    printf("backend new_output: %zu\n", offsetof(struct wlr_backend, events.new_output));
    return 0;
}
