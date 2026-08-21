sed -i 's/char o_file/printf("BEFORE EMIT TO %s\\n", basename); fflush(stdout); char o_file/g' src/codegen_llvm/driver.c
sed -i 's/LLVMVerifyModule(ctx->llvm_mod, LLVMPrintMessageAction, &err_msg);/printf("BEFORE VERIFY\\n"); fflush(stdout); LLVMVerifyModule(ctx->llvm_mod, LLVMPrintMessageAction, &err_msg); printf("AFTER VERIFY\\n"); fflush(stdout);/g' src/codegen_llvm/codegen.c
make -C build -j$(nproc)
./build/alkyl project/wmyl/wmyl.kyl --unopt > debug_log.txt 2>&1
echo "Exit: $?" >> debug_log.txt
tail -n 30 debug_log.txt
