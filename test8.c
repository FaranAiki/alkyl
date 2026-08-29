#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("offset of events.frame: %zu\n", offsetof(struct wlr_output, events.frame));
    return 0;
}
