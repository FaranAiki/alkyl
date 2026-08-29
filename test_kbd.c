#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("events.key: %zu\n", offsetof(struct wlr_keyboard, events.key));
    printf("events.modifiers: %zu\n", offsetof(struct wlr_keyboard, events.modifiers));
    return 0;
}
