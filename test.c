#include <llvm-c/Core.h>
#include <stdio.h>

int main() {
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx, "ns.Clib");
    printf("Before set body: IsOpaque = %d\n", LLVMIsOpaqueStruct(struct_ty));
    LLVMStructSetBody(struct_ty, NULL, 0, 0);
    printf("After set body: IsOpaque = %d\n", LLVMIsOpaqueStruct(struct_ty));
    printf("IsSized = %d\n", LLVMTypeIsSized(struct_ty));
    LLVMContextDispose(ctx);
    return 0;
}
