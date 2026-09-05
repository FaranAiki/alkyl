#define WLR_USE_UNSTABLE
#include <stdio.h>
#include <wlr/types/wlr_keyboard.h>
int main() {
    printf("Modifier offset: %zu\n", offsetof(struct wlr_keyboard, modifiers));
    return 0;
}
