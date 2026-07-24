sed -i 's/LLVMTypeRef \*param_types = malloc(sizeof(LLVMTypeRef) \* param_count);/LLVMTypeRef param_types[param_count];/g' src/codegen_llvm/translate/flow.c
sed -i 's/free(param_types);//g' src/codegen_llvm/translate/flow.c

sed -i 's/LLVMValueRef \*args = malloc(sizeof(LLVMValueRef) \* inst->arg_count);/LLVMValueRef args[inst->arg_count];/g' src/codegen_llvm/translate/flow.c
sed -i 's/LLVMTypeRef \*param_tys = malloc(sizeof(LLVMTypeRef) \* num_params);/LLVMTypeRef param_tys[num_params];/g' src/codegen_llvm/translate/flow.c
sed -i 's/free(args);//g' src/codegen_llvm/translate/flow.c
sed -i 's/free(param_tys);//g' src/codegen_llvm/translate/flow.c
