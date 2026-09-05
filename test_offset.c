#include <stdio.h>
#include <stddef.h>
#include <wlr/types/wlr_keyboard.h>
int main() {
    printf("wlr_keyboard events.key offset = %zu\n", offsetof(struct wlr_keyboard, events.key));
    return 0;
}
