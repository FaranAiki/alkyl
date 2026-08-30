#define WLR_USE_UNSTABLE
#include <stdio.h>
#include <wlr/types/wlr_keyboard.h>
int main() {
    printf("wlr_keyboard_from_input_device is a macro? No.\n");
    return 0;
}
