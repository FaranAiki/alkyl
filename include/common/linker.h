#ifndef LINKER_H
#define LINKER_H

#include <stddef.h>

typedef enum {
    LINKER_GCC,
    LINKER_CLANG,
    LINKER_LLD,
    LINKER_MOLD,
    LINKER_ALYNK,
    LINKER_NONE
} LinkerType;

/**
 * @brief Returns the linker command template for a given linker type.
 * @param type The linker type.
 * @return A printf-style format string for the linker command.
 */
const char* alkyl_get_linker_command(LinkerType type);
/**
 * @brief Links an object file into an executable using the specified linker.
 * @param obj_file The path to the object file.
 * @param output_basename The base name for the output executable.
 * @param link_flags Additional linker flags, or NULL.
 * @param linker_type The linker to use.
 * @return 0 on success, non-zero on failure.
 */
int alkyl_link(const char *obj_file, const char *output_basename, const char *link_flags, LinkerType linker_type);

#endif // LINKER_H
