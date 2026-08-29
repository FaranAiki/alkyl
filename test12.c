#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("sizeof wlr_output_state: %zu\n", sizeof(struct wlr_output_state));
    return 0;
}
