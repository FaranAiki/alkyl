#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <stdio.h>

int main() {
    printf("Starting C wmyl...\n");
    struct wl_display *display = wl_display_create();
    if (!display) { printf("fail 1\n"); return 1; }
    
    struct wlr_backend *backend = wlr_backend_autocreate(display, NULL);
    if (!backend) { printf("fail 2\n"); return 1; }
    
    printf("Success!\n");
    return 0;
}
