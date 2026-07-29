#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stddef.h>

char* get_smart_input(void *arena, int cmd_count, void *sem_ctx);

#endif // KEYBOARD_H
