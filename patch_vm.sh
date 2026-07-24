sed -i 's/long long \*new_args = malloc(sizeof(long long) \* inst->arg_count);/int __na_sz = inst->arg_count > 0 ? inst->arg_count : 1; long long new_args[__na_sz];/g' src/meta/vm.c
sed -i 's/ffi_type \*\*arg_types = malloc(inst->arg_count \* sizeof(ffi_type \*));/int __at_sz = inst->arg_count > 0 ? inst->arg_count : 1; ffi_type \*arg_types[__at_sz];/g' src/meta/vm.c
sed -i 's/void \*\*arg_values = malloc(inst->arg_count \* sizeof(void \*));/int __av_sz = inst->arg_count > 0 ? inst->arg_count : 1; void \*arg_values[__av_sz];/g' src/meta/vm.c
sed -i 's/free(new_args);//g' src/meta/vm.c
sed -i 's/free(arg_types);//g' src/meta/vm.c
sed -i 's/free(arg_values);//g' src/meta/vm.c
