#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("offset of events.key: %zu\n", offsetof(struct wlr_keyboard, events.key));
    return 0;
}
