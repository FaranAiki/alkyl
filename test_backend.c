#include <wlr/backend.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("events.new_input: %zu\n", offsetof(struct wlr_backend, events.new_input));
}
