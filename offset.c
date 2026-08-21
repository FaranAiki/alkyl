#include <stdio.h>
#include <stddef.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>

int main() {
    printf("backend_events: %zu\n", offsetof(struct wlr_backend, events));
    printf("output_events: %zu\n", offsetof(struct wlr_output, events));
    return 0;
}
