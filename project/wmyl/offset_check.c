#include <stdio.h>
#include <stddef.h>
#include <wlr/backend.h>

int main() {
    printf("wlr_backend.events.new_input offset: %zd\n", offsetof(struct wlr_backend, events.new_input));
    printf("wlr_backend.events.new_output offset: %zd\n", offsetof(struct wlr_backend, events.new_output));
    printf("wlr_backend.events.destroy offset: %zd\n", offsetof(struct wlr_backend, events.destroy));
    return 0;
}
