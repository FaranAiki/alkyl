#include "mlir/generate.h"
#include "codegen/codegen.h"
#include <stdio.h>
#include <stdlib.h>

int backend_run_semantic(SemanticCtx *sem_ctx, ASTNode *root, const char *basename, const char *link_flags, int optimization_level, LinkerType linker) {
    (void)link_flags;
    (void)optimization_level;
    (void)linker;
    (void)sem_ctx;

    printf("Backend run invoked for MLIR! (Delegating to mlir_generate...)\n");

    mlir_generate(root, basename);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mlir-opt --convert-scf-to-cf --convert-cf-to-llvm --convert-func-to-llvm --convert-arith-to-llvm --finalize-memref-to-llvm --reconcile-unrealized-casts %s.mlir | mlir-translate --mlir-to-llvmir -o %s.ll", basename, basename);
    int ret = system(cmd);
    if (ret != 0) {
        printf("MLIR lowering to LLVM IR failed\n");
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "clang -Wno-override-module %s.ll -o %s %s", basename, basename, link_flags ? link_flags : "");
    ret = system(cmd);
    if (ret != 0) {
        printf("Clang compilation failed\n");
        return 1;
    }

    return 0;
}
