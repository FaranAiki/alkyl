#include "alir/alir.h"
#include "common/linker.h"
#include <stdio.h>

extern int alkyl_backend_run_cranelift(void *alir_ptr);

int backend_run_alir(AlirModule *module, const char *basename, const char *link_flags, int optimization_level, LinkerType linker) {
    (void)basename;
    (void)link_flags;
    (void)optimization_level;
    (void)linker;

    printf("[Cranelift C Driver] Invoking Rust cranelift backend...\n");
    return alkyl_backend_run_cranelift((void*)module);
}
