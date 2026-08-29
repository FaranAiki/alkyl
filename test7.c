#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("offset of modifiers: %zu\n", offsetof(struct wlr_keyboard, modifiers));
    return 0;
}
