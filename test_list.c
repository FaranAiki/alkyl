#include <wayland-server-core.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    printf("prev: %zu, next: %zu\n", offsetof(struct wl_list, prev), offsetof(struct wl_list, next));
    printf("listener link: %zu, notify: %zu\n", offsetof(struct wl_listener, link), offsetof(struct wl_listener, notify));
    return 0;
}
