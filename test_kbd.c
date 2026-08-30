#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>
int main() {
    printf("modifiers: %zu\n", offsetof(struct wlr_keyboard, events.modifiers));
    printf("key: %zu\n", offsetof(struct wlr_keyboard, events.key));
    printf("keymap: %zu\n", offsetof(struct wlr_keyboard, events.keymap));
    printf("repeat_info: %zu\n", offsetof(struct wlr_keyboard, events.repeat_info));
    return 0;
}
