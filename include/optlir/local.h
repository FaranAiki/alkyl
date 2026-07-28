#ifndef OPTLIR_LOCAL_H
#define OPTLIR_LOCAL_H

#include "../alir/alir.h"

typedef struct ConstVal {
    long long int_val;
    double double_val;
    int is_const;
    int is_float;
} ConstVal;

ConstVal eval_pure_function(AlirModule *module, AlirFunction *func, AlirValue **args, int arg_count, VarType ret_type);

void optlir_local_optimize(AlirModule *module);

#endif
