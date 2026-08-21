#include <stddef.h>
#include <stdio.h>
#include <wlr/backend.h>
int main() {
    printf("new_input offset: %zd\n", offsetof(struct wlr_backend, events.new_input));
    return 0;
}
