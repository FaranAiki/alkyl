#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>
int main() {
    printf("time_msec: %zu\n", offsetof(struct wlr_keyboard_key_event, time_msec));
    printf("keycode: %zu\n", offsetof(struct wlr_keyboard_key_event, keycode));
    printf("update_state: %zu\n", offsetof(struct wlr_keyboard_key_event, update_state));
    printf("state: %zu\n", offsetof(struct wlr_keyboard_key_event, state));
    printf("sizeof: %zu\n", sizeof(struct wlr_keyboard_key_event));
    return 0;
}
