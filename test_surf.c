#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
#include <stdio.h>
int main() {
    struct wlr_xdg_surface surf;
    printf("%p\n", surf.role_data);
    return 0;
}
