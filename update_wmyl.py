import sys

content = """/*
    wlroots wayland window manager
*/

import "std/print";
import "wlroots/core";

@c extern {
    int setenv(char* name, char* value, int overwrite);
    int fork();
    int execl(char* path, char* arg0, char* arg1, char* arg2, void* arg3);
    void _exit(int status);
}

class Server {
    wl_display* display;
    wlr_backend* backend;
    wlr_renderer* renderer;
    wlr_allocator* allocator;
    wlr_compositor* compositor;
    wlr_data_device_manager* manager;
    wlr_output_layout* output_layout;
    wlr_xdg_shell* xdg_shell;
    wlr_seat* seat;
    
    wlroots.wl_listener new_input;
}

let global_server: Server;

void server_new_input(wlroots.wl_listener* listener, void* data) {
    let device = data;
    let type = (device as int*)[0]; // enum wlr_input_device_type is at offset 0
    if (type == 0) { // WLR_INPUT_DEVICE_KEYBOARD
        std.printf("New keyboard detected!\\n");
        let keyboard = wlroots.wlr_keyboard_from_input_device(device);
        wlroots.wlr_seat_set_keyboard(global_server.seat, keyboard);
        wlroots.wlr_seat_set_capabilities(global_server.seat, 1); // WL_SEAT_CAPABILITY_KEYBOARD
    } else if (type == 1) { // WLR_INPUT_DEVICE_POINTER
        std.printf("New pointer detected!\\n");
        wlroots.wlr_seat_set_capabilities(global_server.seat, 2); // WL_SEAT_CAPABILITY_POINTER
    }
}

int main() {
    std.printf("Starting wmyl (wlroots minimal WM)...\\n");

    let display = wlroots.create_display();
    untaint display residue err { return 1; }
    global_server.display = display;

    let socket = wlroots.add_socket_auto(display);
    untaint socket residue err { return 1; }

    std.printf("Running Wayland display on {}...\\n", socket);

    let backend = wlroots.create_backend(display);
    untaint backend residue err { return 1; }
    global_server.backend = backend;

    let renderer = wlroots.create_renderer(backend);
    untaint renderer residue err { return 1; }
    global_server.renderer = renderer;

    wlroots.init_renderer(renderer, display);

    let allocator = wlroots.create_allocator(backend, renderer);
    untaint allocator residue err { return 1; }
    global_server.allocator = allocator;

    let compositor = wlroots.create_compositor(display, renderer);
    untaint compositor residue err { return 1; }
    global_server.compositor = compositor;

    let manager = wlroots.create_data_device_manager(display);
    untaint manager residue err { return 1; }
    global_server.manager = manager;

    let output_layout = wlroots.create_output_layout(display);
    untaint output_layout residue err { return 1; }
    global_server.output_layout = output_layout;

    let xdg_shell = wlroots.create_xdg_shell(display, 3);
    untaint xdg_shell residue err { return 1; }
    global_server.xdg_shell = xdg_shell;

    let seat = wlroots.create_seat(display, "seat0");
    untaint seat residue err { return 1; }
    global_server.seat = seat;

    wlroots.backend_on_new_input(backend, &(global_server.new_input), server_new_input as void*);

    let started = wlroots.start_backend(backend);
    untaint started residue err { return 1; }

    std.printf("Backend started successfully! Spawning kitty...\\n");

    setenv("WAYLAND_DISPLAY", socket, 1);
    let pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "/bin/sh", "-c", "kitty", null as void*);
        _exit(1);
    }

    wlroots.run_display(display);

    wlroots.destroy_clients(display);
    wlroots.destroy_display(display);

    std.printf("wmyl exited gracefully.\\n");
    return 0;
}
"""

with open("project/wmyl/wmyl.kyl", "w") as f:
    f.write(content)

