#include <stdio.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <stddef.h>

int main() {
    printf("Offset of events: %zu\n", offsetof(struct wlr_keyboard, events));
    printf("Offset of events.key: %zu\n", offsetof(struct wlr_keyboard, events.key));
    return 0;
}
