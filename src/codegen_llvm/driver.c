#include "codegen/codegen.h"
#include "codegen_llvm/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

int backend_run(AlirModule *module, const char *basename, const char *link_flags) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    CodegenCtx *cg_ctx = codegen_init(module);
    LLVMModuleRef llvm_module = codegen_generate(cg_ctx);

    char ll_file[1024];
    snprintf(ll_file, sizeof(ll_file), "%s.ll", basename);
    LLVMPrintModuleToFile(llvm_module, ll_file, NULL);
    
    char *error = NULL;
    if (LLVMVerifyModule(llvm_module, LLVMPrintMessageAction, &error)) {
        fprintf(stderr, "LLVM Verification Error: %s\n", error);
        if (error) LLVMDisposeMessage(error);
        return 1;
    }
    if (error) LLVMDisposeMessage(error);

    char *triple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef target;
    char *err_msg = NULL;
    LLVMGetTargetFromTriple(triple, &target, &err_msg);

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
        target, triple, "generic", "",
        LLVMCodeGenLevelAggressive, LLVMRelocPIC, LLVMCodeModelDefault
    );

    char o_file[1024];
    snprintf(o_file, sizeof(o_file), "%s.o", basename);

    if (LLVMTargetMachineEmitToFile(machine, llvm_module, o_file, LLVMObjectFile, &err_msg) != 0) {
        fprintf(stderr, "Emit Error: %s\n", err_msg);
        return 1;
    }

    printf("Compiled to %s\n", o_file);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "gcc -g -O0 %s -o %s -no-pie %s", o_file, basename, link_flags);

    printf("Linking: %s\n", cmd);
    int final_ret = 0;
    int res = system(cmd);
    if (res == 0) {
        printf("Linked to %s\n", basename);
    } else {
        printf("Linking failed.\n");
        final_ret = 1;
    }

    codegen_dispose(cg_ctx); 
    LLVMDisposeTargetMachine(machine);
    LLVMContextRef llvm_ctx = LLVMGetModuleContext(llvm_module);
    LLVMDisposeModule(llvm_module);
    LLVMContextDispose(llvm_ctx);
    LLVMDisposeMessage(triple);

    return final_ret;
}
