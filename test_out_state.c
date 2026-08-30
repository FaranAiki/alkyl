#define WLR_USE_UNSTABLE
#include <stdio.h>
#include <wlr/types/wlr_output.h>
int main() {
    printf("sizeof: %zu\n", sizeof(struct wlr_output_state));
    return 0;
}
