/export namespace wlroots {/a\
    @c extern {\
        void* malloc(long size);\
        void free(void* ptr);\
        wlroots.wlr_output_layout* wlr_output_layout_create(wlroots.wl_display* display);\
        wlroots.wlr_xdg_shell* wlr_xdg_shell_create(wlroots.wl_display* display, int version);\
        void wlr_xdg_toplevel_set_mapped(wlroots.wlr_xdg_toplevel* toplevel, bool mapped);\
        void wlr_output_init_render(wlroots.wlr_output* output, wlroots.wlr_allocator* allocator, wlroots.wlr_renderer* renderer);\
        void wlr_output_state_init(wlroots.wlr_output_state* state);\
        void wlr_output_state_set_enabled(wlroots.wlr_output_state* state, bool enabled);\
        void wlr_output_state_set_mode(wlroots.wlr_output_state* state, void* mode);\
        void* wlr_output_preferred_mode(wlroots.wlr_output* output);\
        void wlr_output_commit_state(wlroots.wlr_output* output, wlroots.wlr_output_state* state);\
        void wlr_output_state_finish(wlroots.wlr_output_state* state);\
        void wlr_scene_xdg_surface_create(wlroots.wlr_scene_tree* tree, wlroots.wlr_xdg_surface* xdg_surface);\
    }
