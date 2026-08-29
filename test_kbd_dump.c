#include <wlr/types/wlr_keyboard.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("sizeof wlr_keyboard: %zu\n", sizeof(struct wlr_keyboard));
    printf("events.key: %zu\n", offsetof(struct wlr_keyboard, events.key));
    return 0;
}
