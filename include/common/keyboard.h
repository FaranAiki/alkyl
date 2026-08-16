#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stddef.h>

/**
 * @brief Reads smart input from the user.
 * @param arena The arena allocator.
 * @param cmd_count The number of commands available.
 * @param sem_ctx The semantic context.
 * @return A newly allocated input string, or NULL.
 */
char* get_smart_input(void *arena, int cmd_count, void *sem_ctx);

#endif // KEYBOARD_H
