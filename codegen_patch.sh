sed -i 's/CodegenCtx \*ctx = calloc(1, sizeof(CodegenCtx));/CodegenCtx \*ctx = mod->compiler_ctx \&\& mod->compiler_ctx->arena ? arena_alloc(mod->compiler_ctx->arena, sizeof(CodegenCtx)) : calloc(1, sizeof(CodegenCtx));\n    if(ctx) memset(ctx, 0, sizeof(CodegenCtx));/g' src/codegen_llvm/codegen.c
sed -i 's/free(ctx);/if (!ctx->arena) free(ctx);/g' src/codegen_llvm/codegen.c
