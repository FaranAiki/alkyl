#include <stdio.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
int main() {
    printf("sizeof wlr_keyboard: %lu\n", sizeof(struct wlr_keyboard));
    printf("offsetof keycodes: %lu\n", offsetof(struct wlr_keyboard, keycodes));
    printf("offsetof events: %lu\n", offsetof(struct wlr_keyboard, events));
    return 0;
}
