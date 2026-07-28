#include <llvm-c/Core.h>
#include <stdio.h>

int main() {
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx);
    
    LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx, "ns.Clib");
    LLVMStructSetBody(struct_ty, NULL, 0, 0);
    
    LLVMTypeRef func_ty = LLVMFunctionType(LLVMVoidTypeInContext(ctx), NULL, 0, 0);
    LLVMValueRef func = LLVMAddFunction(mod, "main", func_ty);
    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(ctx, func, "entry");
    LLVMPositionBuilderAtEnd(builder, bb);
    
    LLVMValueRef alloc = LLVMBuildAlloca(builder, struct_ty, "alloc");
    
    char *err = NULL;
    LLVMPrintModuleToFile(mod, "test.ll", &err);
    if (err) printf("Error: %s\n", err);
    else printf("Success! Check test.ll\n");
    
    LLVMContextDispose(ctx);
    return 0;
}
