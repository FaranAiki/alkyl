#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <stdio.h>
#include <stddef.h>
int main() {
    printf("request_state: %zu\n", offsetof(struct wlr_output, events.request_state));
    return 0;
}
