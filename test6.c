#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("offset of events.modifiers: %zu\n", offsetof(struct wlr_keyboard, events.modifiers));
    return 0;
}
