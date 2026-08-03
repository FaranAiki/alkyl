#include "alir/alir.h"
#include "common/linker.h"
#include <stdio.h>

extern int alkyl_backend_run_cranelift(void *alir_ptr, const char *basename);

// TODO make sure that this is proper
// ONHOLD!
int backend_run_alir(AlirModule *module, const char *basename, const char *link_flags, int optimization_level, LinkerType linker) {
    (void)optimization_level;
    (void)linker;

    printf("[Cranelift C Driver] Invoking Rust cranelift backend...\n");
    int ret = alkyl_backend_run_cranelift((void*)module, basename);
    if (ret != 0) return ret;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc %s.o src/runtime/runtime.c -o %s %s -lm", basename, basename, link_flags);
    printf("[Cranelift C Driver] Linking with: %s\n", cmd);
    return system(cmd);
}
