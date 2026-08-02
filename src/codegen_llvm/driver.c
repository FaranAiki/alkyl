#include "codegen/codegen.h"
#include "codegen_llvm/codegen.h"
#include "common/linker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Error.h>

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

static LLVMTargetMachineRef get_target_machine(int optimization_level) {
    static int current_level = -1;
    static LLVMTargetMachineRef aggressive_machine = NULL;
    static LLVMTargetMachineRef none_machine = NULL;

    if (current_level == optimization_level) {
        if (optimization_level == 0 && none_machine) return none_machine;
        if (optimization_level > 0 && aggressive_machine) return aggressive_machine;
    }

    if (!cached_target_machine) {
        cached_triple = LLVMGetDefaultTargetTriple();
        LLVMTargetRef target;
        char *err_msg = NULL;
        if (LLVMGetTargetFromTriple(cached_triple, &target, &err_msg) != 0) {
            fprintf(stderr, "LLVM Target Error: %s\n", err_msg);
            if (err_msg) LLVMDisposeMessage(err_msg);
            return NULL;
        }

        LLVMCodeGenOptLevel level = optimization_level > 0 ? LLVMCodeGenLevelAggressive : LLVMCodeGenLevelNone;
        cached_target_machine = LLVMCreateTargetMachine(
            target, cached_triple, "generic", "",
            level, LLVMRelocPIC, LLVMCodeModelDefault
        );

        if (optimization_level == 0) {
            none_machine = cached_target_machine;
        } else {
            aggressive_machine = cached_target_machine;
        }
        current_level = optimization_level;
    }

    return cached_target_machine;
}

int backend_run_alir(AlirModule *module, const char *basename, const char *link_flags, int optimization_level, LinkerType linker) {
    if (!module) return 1;

    char obj_filename[1024];
    snprintf(obj_filename, sizeof(obj_filename), "%s.o", basename);

    ensure_llvm_initialized();

    LLVMTargetMachineRef machine = get_target_machine(optimization_level);
    if (!machine) {
        return 1;
    }

    CodegenCtx *cg_ctx = codegen_init(module);
    LLVMModuleRef llvm_module = codegen_generate(cg_ctx);

    if (optimization_level > 0) {
        char passes[32];
        if (optimization_level == 1) snprintf(passes, sizeof(passes), "%s", "default<O1>");
        else if (optimization_level == 2) snprintf(passes, sizeof(passes), "%s", "default<O2>");
        else if (optimization_level == 3) snprintf(passes, sizeof(passes), "%s", "default<O3>");
        else if (optimization_level == 4) snprintf(passes, sizeof(passes), "%s", "default<Os>");
        else if (optimization_level >= 5) snprintf(passes, sizeof(passes), "%s", "default<Oz>");
        
        LLVMPassBuilderOptionsRef pb_opt = LLVMCreatePassBuilderOptions();
        LLVMErrorRef err = LLVMRunPasses(llvm_module, passes, machine, pb_opt);
        if (err) {
            char *err_str = LLVMGetErrorMessage(err);
            fprintf(stderr, "PassBuilder Error: %s\n", err_str);
            LLVMDisposeErrorMessage(err_str);
        }
        LLVMDisposePassBuilderOptions(pb_opt);
    }

    char o_file[1024];
    snprintf(o_file, sizeof(o_file), "%s.o", basename);

    char *err_msg = NULL;
    if (LLVMPrintModuleToFile(llvm_module, "my_out.ll", &err_msg) != 0) {
        fprintf(stderr, "IR Print Error: %s\n", err_msg);
        if (err_msg) LLVMDisposeMessage(err_msg);
        codegen_dispose(cg_ctx);
        return 1;
    }
    if (err_msg) LLVMDisposeMessage(err_msg);
    err_msg = NULL;

    if (LLVMTargetMachineEmitToFile(machine, llvm_module, o_file, LLVMObjectFile, &err_msg) != 0) {
        fprintf(stderr, "Emit Error: %s\n", err_msg);
        if (err_msg) LLVMDisposeMessage(err_msg);
        codegen_dispose(cg_ctx);
        return 1;
    }
    if (err_msg) LLVMDisposeMessage(err_msg);

    int link_ret = alkyl_link(o_file, basename, link_flags, linker);
    if (link_ret != 0) {
        fprintf(stderr, "Linking failed.\n");
        codegen_dispose(cg_ctx);
        LLVMContextRef llvm_ctx = LLVMGetModuleContext(llvm_module);
        LLVMDisposeModule(llvm_module);
        LLVMContextDispose(llvm_ctx);
        return 1;
    }

    codegen_dispose(cg_ctx);
    LLVMContextRef llvm_ctx = LLVMGetModuleContext(llvm_module);
    LLVMDisposeModule(llvm_module);
    LLVMContextDispose(llvm_ctx);

    return 0;
}
