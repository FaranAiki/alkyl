#include "mlir/generate.h"
#include "codegen/codegen.h"
#include <stdio.h>

int backend_run(AlirModule *module, const char *basename, const char *link_flags, int optimization_level, LinkerType linker) {
    (void)link_flags;
    (void)optimization_level;
    (void)linker;
    
    // In MLIR, we actually don't use ALIR! We extract the semantic context from the module
    // if possible, or assume it's available. 
    // For a real AST backend swap, main.c would pass SemanticCtx directly.
    // Here we'll just mock it so the linker is happy for the tests.
    
    printf("Backend run invoked for MLIR! (Delegating to mlir_generate...)\n");
    
    // As a mock, we pass NULL since main.c currently only passes ALIR down here.
    mlir_generate(NULL, basename);
    
    return 0;
}
