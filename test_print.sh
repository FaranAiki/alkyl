sed -i 's/LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->llvm_ctx, st->name);/LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->llvm_ctx, st->name); printf("Found struct: %s\\n", st->name);/' src/codegen_llvm/codegen.c
cd build && make -j4 && cd .. && build/alkyl_llvm test/code/extern/test_nested_class2.aky
git checkout src/codegen_llvm/codegen.c
