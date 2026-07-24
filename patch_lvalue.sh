sed -i 's/param_names = calloc(num_params, sizeof(char\*));/param_names = alir_alloc(ctx, num_params \* sizeof(char\*));/g' src/alir/lvalue.c
sed -i 's/param_args = calloc(num_params, sizeof(ASTNode\*));/param_args = alir_alloc(ctx, num_params \* sizeof(ASTNode\*));/g' src/alir/lvalue.c
sed -i 's/free(param_names);//g' src/alir/lvalue.c
sed -i 's/free(param_args);//g' src/alir/lvalue.c
