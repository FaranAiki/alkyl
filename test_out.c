#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <stdio.h>
#include <stddef.h>
int main() {
    printf("output.events.frame = %zu\n", offsetof(struct wlr_output, events.frame));
    return 0;
}
