#include <llvm-c/Core.h>
#include <stdio.h>

int main() {
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx);

    LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx, "MyStruct");
    LLVMTypeRef i32_ty = LLVMInt32TypeInContext(ctx);
    LLVMStructSetBody(struct_ty, &i32_ty, 1, 0);

    LLVMTypeRef func_ty = LLVMFunctionType(LLVMVoidTypeInContext(ctx), NULL, 0, 0);
    LLVMValueRef func = LLVMAddFunction(mod, "main", func_ty);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx, func, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef alloc = LLVMBuildAlloca(builder, struct_ty, "alloc");
    LLVMBuildRetVoid(builder);

    LLVMDumpModule(mod);
    return 0;
}
