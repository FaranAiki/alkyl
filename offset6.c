#include <stddef.h>
#include <stdio.h>
#include <wlr/types/wlr_input_device.h>
int main() {
    struct wlr_input_device dev;
    printf("wlr_keyboard* offset: %zd\n", offsetof(struct wlr_input_device, keyboard));
    return 0;
}
