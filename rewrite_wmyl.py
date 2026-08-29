import re

with open("project/wmyl/wmyl.kyl", "r") as f:
    content = f.read()

# Grouping externs
externs = """@c extern {
    int setenv(char* name, char* value, int overwrite);
    int fork();
    int execl(char* path, char* arg0, char* arg1, char* arg2, void* arg3);
    void _exit(int status);

    void* malloc(long size);
    void free(void* ptr);
    void wlr_output_layout_add_auto(void* layout, void* output);
    void* wlr_scene_create();
    void* wlr_scene_output_create(void* scene, void* output);
    void wlr_scene_attach_output_layout(void* scene, void* layout);
    void wlr_scene_output_commit(void* scene_output, void* opt);
    void wlr_scene_output_send_frame_done(void* scene_output, void* now);
    void clock_gettime(int clk_id, void* tp);
    void wl_display_terminate(void* display);
    int wlr_keyboard_get_modifiers(void* keyboard);
}"""

content = re.sub(r'(@c extern.*?\n)+', externs + "\n", content, count=1)

# Modify Server class
server_class_old = """abstract container class Server {
    void* new_input; // wl_listener must be first for container_of hack
    void* new_output;
    void* new_xdg_surface;
    void* display;
    void* backend;
    void* renderer;
    void* allocator;
    void* compositor;
    void* manager;
    void* subcompositor;
    void* output_layout;
    void* xdg_shell;
    void* seat;
    void* scene;
}"""

server_class_new = """abstract container class Server {
    void* new_input; // wl_listener must be first for container_of hack
    void* new_output;
    void* new_xdg_surface;
    void* display;
    void* backend;
    void* renderer;
    void* allocator;
    void* compositor;
    void* manager;
    void* subcompositor;
    void* output_layout;
    void* xdg_shell;
    void* seat;
    void* scene;

    void on_new_xdg_surface(void* xdg_surface) {
        std.printf("New XDG Surface requested!\\n");
        wlroots.wlr_scene_add_xdg_surface(this.scene, xdg_surface);
        wlroots.focus_xdg_surface(this.seat, xdg_surface);
    }

    void on_new_input(void* device) {
        let type = (device as wlroots.wlr_input_device*).type;
        if (type == 0) { // WLR_INPUT_DEVICE_KEYBOARD
            std.printf("New keyboard detected!\\n");
            let keyboard = wlroots.wlr_keyboard_from_input_device(device);
            wlroots.wlr_seat_set_keyboard(this.seat, keyboard);
            wlroots.wlr_seat_set_capabilities(this.seat, 1); // WL_SEAT_CAPABILITY_KEYBOARD

            let ac = heap.ArenaAllocator(null, 65536);
            let kb = ac.alloc[Keyboard]();
            untaint kb residue err { return; }
            kb.server = this as void*;
            kb.wlr_keyboard = keyboard;

            let key_listener = ac.alloc[Listener]();
            untaint key_listener residue err { return; }
            key_listener.data = kb as void*;
            kb.key = key_listener as void*;
            
            let signal_key = ((keyboard as long) + 304) as wlroots.wl_signal*;
            let lst_key = key_listener as wlroots.wl_listener*;
            wlroots.init_listener(lst_key, keyboard_handle_key as void*);
            wlroots.wl_signal_add(signal_key, lst_key);

            let modifiers_listener = ac.alloc[Listener]();
            untaint modifiers_listener residue err { return; }
            modifiers_listener.data = kb as void*;
            kb.modifiers = modifiers_listener as void*;
            
            let signal_modifiers = ((keyboard as long) + 320) as wlroots.wl_signal*;
            let lst_modifiers = modifiers_listener as wlroots.wl_listener*;
            wlroots.init_listener(lst_modifiers, keyboard_handle_modifiers as void*);
            wlroots.wl_signal_add(signal_modifiers, lst_modifiers);
        } else if (type == 1) { // WLR_INPUT_DEVICE_POINTER
            std.printf("New pointer detected!\\n");
            wlroots.wlr_seat_set_capabilities(this.seat, 2); // WL_SEAT_CAPABILITY_POINTER
        }
    }

    void on_new_output(void* wlr_output) {
        std.printf("New output connected!\\n");
        wlroots.wlr_output_init_render(wlr_output as wlroots.wlr_output*, this.allocator as wlroots.wlr_allocator*, this.renderer as wlroots.wlr_renderer*);

        let state = malloc(128) as wlroots.wlr_output_state*;
        wlroots.wlr_output_state_init(state);
        wlroots.wlr_output_state_set_enabled(state, 1);

        let mode = wlroots.wlr_output_preferred_mode(wlr_output as wlroots.wlr_output*);
        if (mode != null) {
            wlroots.wlr_output_state_set_mode(state, mode);
        }

        wlroots.wlr_output_commit_state(wlr_output as wlroots.wlr_output*, state);
        wlroots.wlr_output_state_finish(state);
        free(state as void*);

        wlr_output_layout_add_auto(this.output_layout, wlr_output);

        let scene_output = wlr_scene_output_create(this.scene, wlr_output);

        let ac = heap.ArenaAllocator(null, 65536);
        let output = ac.alloc[Output]();
        untaint output residue err { return; }
        output.server = this as void*;
        output.wlr_output = wlr_output;
        output.scene_output = scene_output;

        let frame_listener = ac.alloc[Listener]();
        untaint frame_listener residue err { return; }
        frame_listener.data = output as void*;
        output.frame_event = frame_listener as void*;

        let signal_frame = ((wlr_output as long) + 168) as wlroots.wl_signal*;
        let lst_frame = frame_listener as wlroots.wl_listener*;
        wlroots.init_listener(lst_frame, output_frame as void*);
        wlroots.wl_signal_add(signal_frame, lst_frame);

        wlr_scene_output_commit(scene_output, null); // Kick off the first frame
    }
}"""
content = content.replace(server_class_old, server_class_new)


output_class_old = """abstract container class Output {
    void* frame_event; // wl_listener
    void* server;
    void* wlr_output;
    void* scene_output;
}"""
output_class_new = """abstract container class Output {
    void* frame_event; // wl_listener
    void* server;
    void* wlr_output;
    void* scene_output;

    void on_frame() {
        wlr_scene_output_commit(this.scene_output, null);

        let timespec = malloc(16);
        clock_gettime(1, timespec); // 1 = CLOCK_MONOTONIC
        wlr_scene_output_send_frame_done(this.scene_output, timespec);
        free(timespec);
    }
}"""
content = content.replace(output_class_old, output_class_new)

keyboard_class_old = """abstract container class Keyboard {
    void* key; // wl_listener
    void* modifiers; // wl_listener
    void* server;
    void* wlr_keyboard;
}"""
keyboard_class_new = """abstract container class Keyboard {
    void* key; // wl_listener
    void* modifiers; // wl_listener
    void* server;
    void* wlr_keyboard;

    void on_key(void* event_data) {
        let server = this.server as Server*;
        let event = event_data as int*;
        let keycode = event[1];
        let state = event[3];

        let modifiers = wlr_keyboard_get_modifiers(this.wlr_keyboard);
        if (state == 1 && keycode == 16 && (modifiers & 64) != 0) {
            std.printf("Super+Q pressed. Exiting...\\n");
            wl_display_terminate(server.display);
            return;
        }

        wlroots.keyboard_notify_key(server.seat, event_data);
    }

    void on_modifiers() {
        let server = this.server as Server*;
        let modifiers_ptr = ((this.wlr_keyboard as long) + 280) as void*;
        wlroots.wlr_seat_keyboard_notify_modifiers(server.seat, modifiers_ptr);
    }
}"""
content = content.replace(keyboard_class_old, keyboard_class_new)

# Replace the C handlers
handlers_old = """void keyboard_handle_key(void* listener_ptr, void* data) {
    Listener *listener = listener_ptr;
    Keyboard *kb = listener.data;
    Server *server = kb.server as Server*;

    wlroots.keyboard_notify_key(server.seat, data);
}

void keyboard_handle_modifiers(void* listener_ptr, void* data) {
    Listener *listener = listener_ptr;
    Keyboard *kb = listener.data;
    Server *server = kb.server as Server*;

    // In wlroots 0.18, wlr_keyboard_set_modifiers might be called or we can just pass the modifiers struct from wlr_keyboard.
    // Actually, wlr_seat_keyboard_notify_modifiers(seat, &keyboard->modifiers)
    let wlr_keyboard = kb.wlr_keyboard;
    let modifiers_ptr = ((wlr_keyboard as long) + 280) as void*;
    wlroots.wlr_seat_keyboard_notify_modifiers(server.seat, modifiers_ptr);
}

void server_new_input(void* listener_ptr, void* data) {
    Listener *listener = listener_ptr;
    Server *server = listener.data;
    let device = data;
    let type = (device as wlroots.wlr_input_device*).type;
    if (type == 0) { // WLR_INPUT_DEVICE_KEYBOARD
        std.printf("New keyboard detected!\\n");
        let keyboard = wlroots.wlr_keyboard_from_input_device(device);
        wlroots.wlr_seat_set_keyboard(server.seat, keyboard);
        wlroots.wlr_seat_set_capabilities(server.seat, 1); // WL_SEAT_CAPABILITY_KEYBOARD

        let ac = heap.ArenaAllocator(null, 65536);
        let kb = ac.alloc[Keyboard]();
        untaint kb residue err { return; }
        kb.server = server as void*;
        kb.wlr_keyboard = keyboard;

        let key_listener = ac.alloc[Listener]();
        untaint key_listener residue err { return; }
        key_listener.data = kb as void*;
        kb.key = key_listener as void*;
        
        let signal_key = ((keyboard as long) + 304) as wlroots.wl_signal*;
        let lst_key = key_listener as wlroots.wl_listener*;
        wlroots.init_listener(lst_key, keyboard_handle_key as void*);
        wlroots.wl_signal_add(signal_key, lst_key);

        let modifiers_listener = ac.alloc[Listener]();
        untaint modifiers_listener residue err { return; }
        modifiers_listener.data = kb as void*;
        kb.modifiers = modifiers_listener as void*;
        
        let signal_modifiers = ((keyboard as long) + 320) as wlroots.wl_signal*;
        let lst_modifiers = modifiers_listener as wlroots.wl_listener*;
        wlroots.init_listener(lst_modifiers, keyboard_handle_modifiers as void*);
        wlroots.wl_signal_add(signal_modifiers, lst_modifiers);
    } else if (type == 1) { // WLR_INPUT_DEVICE_POINTER
        std.printf("New pointer detected!\\n");
        wlroots.wlr_seat_set_capabilities(server.seat, 2); // WL_SEAT_CAPABILITY_POINTER
    }
}

void output_frame(void* listener_ptr, void* data) {
    Listener *listener = listener_ptr;
    Output *output = listener.data;
    wlr_scene_output_commit(output.scene_output, null);

    let timespec = malloc(16);
    clock_gettime(1, timespec); // 1 = CLOCK_MONOTONIC
    wlr_scene_output_send_frame_done(output.scene_output, timespec);
    free(timespec);
}

void server_new_output(void* listener_ptr, void* data) {
    Listener *listener = listener_ptr;
    Server *server = listener.data;
    let wlr_output = data;

    std.printf("New output connected!\\n");
    wlroots.wlr_output_init_render(wlr_output as wlroots.wlr_output*, server.allocator as wlroots.wlr_allocator*, server.renderer as wlroots.wlr_renderer*);

    let state = malloc(128) as wlroots.wlr_output_state*;
    wlroots.wlr_output_state_init(state);
    wlroots.wlr_output_state_set_enabled(state, 1);

    let mode = wlroots.wlr_output_preferred_mode(wlr_output as wlroots.wlr_output*);
    if (mode != null) {
        wlroots.wlr_output_state_set_mode(state, mode);
    }

    wlroots.wlr_output_commit_state(wlr_output as wlroots.wlr_output*, state);
    wlroots.wlr_output_state_finish(state);
    free(state as void*);

    wlr_output_layout_add_auto(server.output_layout, wlr_output);

    let scene_output = wlr_scene_output_create(server.scene, wlr_output);

    let ac = heap.ArenaAllocator(null, 65536);
    let output = ac.alloc[Output]();
    untaint output residue err { return; }
    output.server = server as void*;
    output.wlr_output = wlr_output;
    output.scene_output = scene_output;

    let frame_listener = ac.alloc[Listener]();
    untaint frame_listener residue err { return; }
    frame_listener.data = output as void*;
    output.frame_event = frame_listener as void*;

    let signal_frame = ((wlr_output as long) + 168) as wlroots.wl_signal*;
    let lst_frame = frame_listener as wlroots.wl_listener*;
    wlroots.init_listener(lst_frame, output_frame as void*);
    wlroots.wl_signal_add(signal_frame, lst_frame);

    wlr_scene_output_commit(scene_output, null); // Kick off the first frame
}

void server_new_xdg_surface(void* listener_ptr, void* data) {
    Listener *listener = listener_ptr;
    Server *server = listener.data;
    let xdg_surface = data;

    std.printf("New XDG Surface requested!\\n");
    // using the bridge function which maps it automatically
    wlroots.wlr_scene_add_xdg_surface(server.scene, xdg_surface);
}"""

handlers_new = """void keyboard_handle_key(void* listener_ptr, void* data) {
    let listener = listener_ptr as Listener*;
    let kb = listener.data as Keyboard*;
    kb.on_key(data);
}

void keyboard_handle_modifiers(void* listener_ptr, void* data) {
    let listener = listener_ptr as Listener*;
    let kb = listener.data as Keyboard*;
    kb.on_modifiers();
}

void server_new_input(void* listener_ptr, void* data) {
    let listener = listener_ptr as Listener*;
    let server = listener.data as Server*;
    server.on_new_input(data);
}

void output_frame(void* listener_ptr, void* data) {
    let listener = listener_ptr as Listener*;
    let output = listener.data as Output*;
    output.on_frame();
}

void server_new_output(void* listener_ptr, void* data) {
    let listener = listener_ptr as Listener*;
    let server = listener.data as Server*;
    server.on_new_output(data);
}

void server_new_xdg_surface(void* listener_ptr, void* data) {
    let listener = listener_ptr as Listener*;
    let server = listener.data as Server*;
    server.on_new_xdg_surface(data);
}"""

content = content.replace(handlers_old, handlers_new)

with open("project/wmyl/wmyl.kyl", "w") as f:
    f.write(content)
