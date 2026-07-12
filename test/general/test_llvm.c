#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <stdio.h>
int main() {
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef d_ty = LLVMDoubleTypeInContext(ctx);
    LLVMTargetDataRef td = LLVMCreateTargetData("");
    unsigned long long size = LLVMABISizeOfType(td, d_ty);
    printf("size: %llu\n", size);
    return 0;
}
