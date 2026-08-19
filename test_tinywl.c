#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
void test(struct wlr_scene_tree *tree, struct wlr_xdg_surface *xdg_surface) {
    struct wlr_scene_tree *node = wlr_scene_xdg_surface_create(tree, xdg_surface);
}
