#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>
int main() {
    printf("wlr_keyboard size = %zu\n", sizeof(struct wlr_keyboard));
    printf("events.key offset = %zu\n", offsetof(struct wlr_keyboard, events.key));
    return 0;
}
