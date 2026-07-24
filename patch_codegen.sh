sed -i 's/LLVMTypeRef \*field_tys = malloc(sizeof(LLVMTypeRef) \* st->field_count);/int __ft_sz = st->field_count > 0 ? st->field_count : 1; LLVMTypeRef field_tys[__ft_sz];/g' src/codegen_llvm/codegen.c
sed -i 's/free(field_tys);//g' src/codegen_llvm/codegen.c

sed -i 's/param_tys = malloc(sizeof(LLVMTypeRef) \* func->param_count);/int __pt2_sz = func->param_count > 0 ? func->param_count : 1; LLVMTypeRef __pt2_arr[__pt2_sz]; param_tys = __pt2_arr;/g' src/codegen_llvm/codegen.c
sed -i 's/free(param_tys);//g' src/codegen_llvm/codegen.c
