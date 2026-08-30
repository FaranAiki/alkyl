#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_input_device.h>
#include <stdio.h>
int main() {
    printf("KEYBOARD: %d\n", WLR_INPUT_DEVICE_KEYBOARD);
    printf("POINTER: %d\n", WLR_INPUT_DEVICE_POINTER);
    return 0;
}
