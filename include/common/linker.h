#ifndef LINKER_H
#define LINKER_H

#include <stddef.h>

typedef enum {
    LINKER_GCC,
    LINKER_CLANG,
    LINKER_LLD,
    LINKER_MOLD
} LinkerType;

const char* alkyl_get_linker_command(LinkerType type);
int alkyl_link(const char *obj_file, const char *output_basename, const char *link_flags, LinkerType linker_type);

#endif
