#include <wayland-server-core.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("prev: %zu, next: %zu\n", offsetof(struct wl_list, prev), offsetof(struct wl_list, next));
    return 0;
}
