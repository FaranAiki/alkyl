#include <stdio.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>

int main() {
    printf("wlr_keyboard.events.key offset: %lu\n", __builtin_offsetof(struct wlr_keyboard, events.key));
    printf("wlr_keyboard.modifiers offset: %lu\n", __builtin_offsetof(struct wlr_keyboard, modifiers));
    printf("wlr_output.events.frame offset: %lu\n", __builtin_offsetof(struct wlr_output, events.frame));
    return 0;
}
