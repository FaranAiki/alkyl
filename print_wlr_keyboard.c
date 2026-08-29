#include <stdio.h>
#include <stddef.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
int main() {
    printf("impl: %lu\n", offsetof(struct wlr_keyboard, impl));
    printf("group: %lu\n", offsetof(struct wlr_keyboard, group));
    printf("keymap_string: %lu\n", offsetof(struct wlr_keyboard, keymap_string));
    printf("keymap_size: %lu\n", offsetof(struct wlr_keyboard, keymap_size));
    printf("keymap_fd: %lu\n", offsetof(struct wlr_keyboard, keymap_fd));
    printf("keymap: %lu\n", offsetof(struct wlr_keyboard, keymap));
    printf("xkb_state: %lu\n", offsetof(struct wlr_keyboard, xkb_state));
    printf("led_indexes: %lu\n", offsetof(struct wlr_keyboard, led_indexes));
    printf("mod_indexes: %lu\n", offsetof(struct wlr_keyboard, mod_indexes));
    printf("leds: %lu\n", offsetof(struct wlr_keyboard, leds));
    printf("keycodes: %lu\n", offsetof(struct wlr_keyboard, keycodes));
    printf("num_keycodes: %lu\n", offsetof(struct wlr_keyboard, num_keycodes));
    printf("modifiers: %lu\n", offsetof(struct wlr_keyboard, modifiers));
    printf("repeat_info: %lu\n", offsetof(struct wlr_keyboard, repeat_info));
    printf("events: %lu\n", offsetof(struct wlr_keyboard, events));
    printf("data: %lu\n", offsetof(struct wlr_keyboard, data));
    return 0;
}
