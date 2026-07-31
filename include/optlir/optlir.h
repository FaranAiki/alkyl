#ifndef OPTLIR_H
#define OPTLIR_H

#include "../alir/alir.h"

typedef struct UsedSet {
    char **names;
    int count;
    int capacity;
} UsedSet;

typedef struct ConstVal {
    long long int_val;
    double double_val;
    int is_const;
    int is_float;
} ConstVal;

void optlir_remove_unused(AlirModule *module);
void optlir_optimize(AlirModule *module, int opt_level);
void optlir_mem2reg_local(AlirModule *module);
void optlir_dce_allocs(AlirModule *module);

#endif
