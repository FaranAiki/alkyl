#include <stdio.h>
#include <stddef.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>

int main() {
    printf("time_msec: %zd\n", offsetof(struct wlr_keyboard_key_event, time_msec));
    printf("keycode: %zd\n", offsetof(struct wlr_keyboard_key_event, keycode));
    printf("state: %zd\n", offsetof(struct wlr_keyboard_key_event, state));
    return 0;
}
