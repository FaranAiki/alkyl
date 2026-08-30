#define WLR_USE_UNSTABLE
#include <stdio.h>
#include <stddef.h>
#include <wlr/types/wlr_keyboard.h>
int main() {
    printf("kbd.key: %zu\n", offsetof(struct wlr_keyboard, events.key));
    printf("kbd.modifiers: %zu\n", offsetof(struct wlr_keyboard, events.modifiers));
    return 0;
}
