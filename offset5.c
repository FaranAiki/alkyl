#include <stddef.h>
#include <stdio.h>
#include <wlr/types/wlr_input_device.h>
int main() {
    printf("wlr_input_device.type offset: %zd\n", offsetof(struct wlr_input_device, type));
    printf("WLR_INPUT_DEVICE_KEYBOARD: %d\n", WLR_INPUT_DEVICE_KEYBOARD);
    printf("WLR_INPUT_DEVICE_POINTER: %d\n", WLR_INPUT_DEVICE_POINTER);
    return 0;
}
