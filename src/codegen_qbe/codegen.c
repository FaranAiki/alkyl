#include "codegen/codegen.h"
#include <stdio.h>
#include <stdlib.h>

int backend_run(AlirModule *module, const char *basename, const char *link_flags) {
    (void)module;
    (void)basename;
    (void)link_flags;
    fprintf(stderr, "QBE backend is ongoing and not fully implemented yet.\n");
    return 1;
}
