/**
 * @file emitter.c
 * @brief LLVM emitter implementation.
 */
#include "../../include/codegen_llvm/codegen.h"
#include "../../include/common/hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void codegen_emit_to_file(CodegenCtx *ctx, const char *filename) {
    if (!ctx || !ctx->llvm_mod) return;
    char *err_msg = NULL;
    LLVMPrintModuleToFile(ctx->llvm_mod, filename, &err_msg);
    if (err_msg) {
        fprintf(stderr, "LLVM File Emission Error: %s\n", err_msg);
        LLVMDisposeMessage(err_msg);
    }
}

void codegen_print(CodegenCtx *ctx) {
    if (!ctx || !ctx->llvm_mod) return;
    char *ir = LLVMPrintModuleToString(ctx->llvm_mod);
    printf("%s", ir);
    LLVMDisposeMessage(ir);
}
