#include <stdio.h>
#include <wlr/types/wlr_keyboard.h>
int main() {
    printf("C wlr_keyboard size: %zu\n", sizeof(struct wlr_keyboard));
    printf("C offset of events.key: %zu\n", offsetof(struct wlr_keyboard, events.key));
    return 0;
}
