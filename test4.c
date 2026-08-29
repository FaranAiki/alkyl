#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("size of keyboard: %zu\n", sizeof(struct wlr_keyboard));
    printf("offset of events: %zu\n", offsetof(struct wlr_keyboard, events));
    printf("offset of events.key: %zu\n", offsetof(struct wlr_keyboard, events.key));
    return 0;
}
