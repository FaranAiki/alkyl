#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#include <stdio.h>
#include <stddef.h>
int main() {
    printf("scene_output.destroy: %zu\n", offsetof(struct wlr_scene_output, events.destroy));
    return 0;
}
