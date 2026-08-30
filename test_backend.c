#define WLR_USE_UNSTABLE
#include <stdio.h>
#include <stddef.h>
#include <wlr/backend.h>
int main() {
    printf("backend.new_input: %zu\n", offsetof(struct wlr_backend, events.new_input));
    printf("backend.new_output: %zu\n", offsetof(struct wlr_backend, events.new_output));
    return 0;
}
