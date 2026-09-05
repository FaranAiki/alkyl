#include <stdio.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_input_device.h>

int main() {
    printf("WLR_INPUT_DEVICE_KEYBOARD = %d\n", WLR_INPUT_DEVICE_KEYBOARD);
    return 0;
}
