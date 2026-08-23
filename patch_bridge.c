--- lib/wlroots/bridge.c
+++ lib/wlroots/bridge.c
@@ -47,3 +47,19 @@
     }
 }
+
+void wlr_keyboard_on_key(struct wlr_keyboard *keyboard, struct wl_listener *listener, void* notify) {
+    listener->notify = notify;
+    wl_signal_add(&keyboard->events.key, listener);
+}
+
+void wlr_keyboard_on_modifiers(struct wlr_keyboard *keyboard, struct wl_listener *listener, void* notify) {
+    listener->notify = notify;
+    wl_signal_add(&keyboard->events.modifiers, listener);
+}
+
+void keyboard_notify_key(struct wlr_seat *seat, struct wlr_keyboard_key_event *event) {
+    wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
+}
