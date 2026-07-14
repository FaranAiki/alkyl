#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <stdio.h>

int main() {
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    
    LLVMTypeRef i32_ty = LLVMInt32TypeInContext(ctx);
    LLVMTypeRef ptr_ty = LLVMPointerType(LLVMInt8TypeInContext(ctx), 0);
    
    LLVMTypeRef struct_tys[] = { i32_ty, ptr_ty };
    LLVMTypeRef class_ty = LLVMPointerType(LLVMInt8TypeInContext(ctx), 1);
    // LLVMStructSetBody(class_ty, struct_tys, 2, 0); // Opaque
    
    LLVMValueRef len_val = LLVMConstInt(i32_ty, 5, 0);
    LLVMValueRef ptr_val = LLVMConstPointerNull(ptr_ty);
    LLVMValueRef vals[] = { len_val, ptr_val };
    
    LLVMValueRef init = LLVMConstStructInContext(ctx, vals, 2, 0);
    
    LLVMValueRef g = LLVMAddGlobal(mod, class_ty, "str0");
    LLVMSetInitializer(g, init);
    
    LLVMDumpModule(mod);
    
    char *err = NULL;
    LLVMVerifyModule(mod, LLVMPrintMessageAction, &err);
    
    return 0;
}
