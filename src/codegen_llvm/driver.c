#include "codegen/codegen.h"
#include "codegen_llvm/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

static int llvm_native_initialized = 0;
static LLVMTargetMachineRef cached_target_machine = NULL;
static char *cached_triple = NULL;

static void ensure_llvm_initialized(void) {
    if (!llvm_native_initialized) {
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMInitializeNativeAsmParser();
        llvm_native_initialized = 1;
    }
}

static LLVMTargetMachineRef get_target_machine(void) {
    if (!cached_target_machine) {
        cached_triple = LLVMGetDefaultTargetTriple();
        LLVMTargetRef target;
        char *err_msg = NULL;
        if (LLVMGetTargetFromTriple(cached_triple, &target, &err_msg) != 0) {
            fprintf(stderr, "LLVM Target Error: %s\n", err_msg);
            if (err_msg) LLVMDisposeMessage(err_msg);
            return NULL;
        }
        cached_target_machine = LLVMCreateTargetMachine(
            target, cached_triple, "generic", "",
            LLVMCodeGenLevelAggressive, LLVMRelocPIC, LLVMCodeModelDefault
        );
    }
    return cached_target_machine;
}

int backend_run(AlirModule *module, const char *basename, const char *link_flags) {
    ensure_llvm_initialized();

    LLVMTargetMachineRef machine = get_target_machine();
    if (!machine) {
        return 1;
    }

    CodegenCtx *cg_ctx = codegen_init(module);
    LLVMModuleRef llvm_module = codegen_generate(cg_ctx);

    char o_file[1024];
    snprintf(o_file, sizeof(o_file), "%s.o", basename);

    char *err_msg = NULL;
    if (LLVMTargetMachineEmitToFile(machine, llvm_module, o_file, LLVMObjectFile, &err_msg) != 0) {
        fprintf(stderr, "Emit Error: %s\n", err_msg);
        if (err_msg) LLVMDisposeMessage(err_msg);
        codegen_dispose(cg_ctx);
        return 1;
    }
    if (err_msg) LLVMDisposeMessage(err_msg);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "gcc -g -O0 %s -o %s -no-pie %s", o_file, basename, link_flags);

    int final_ret = 0;
    int res = system(cmd);
    if (res != 0) {
        fprintf(stderr, "Linking failed.\n");
        final_ret = 1;
    }

    codegen_dispose(cg_ctx); 
    LLVMContextRef llvm_ctx = LLVMGetModuleContext(llvm_module);
    LLVMDisposeModule(llvm_module);
    LLVMContextDispose(llvm_ctx);

    return final_ret;
}
