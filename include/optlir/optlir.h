/**
 * @file optlir.h
 * @brief ALIR optimization passes.
 */
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

/**
 * @brief Removes unused functions and globals from the module.
 * @param module The ALIR module.
 */
void optlir_remove_unused(AlirModule *module);

/**
 * @brief Runs all optimization passes on the module.
 * @param module The ALIR module.
 * @param opt_level The optimization level (0=none, 1=basic, 2=aggressive).
 */
void optlir_optimize(AlirModule *module, int opt_level);

/**
 * @brief Runs the mem2reg local optimization pass.
 * @param module The ALIR module.
 */
void optlir_mem2reg_local(AlirModule *module);

/**
 * @brief Removes dead alloc instructions.
 * @param module The ALIR module.
 */
void optlir_dce_allocs(AlirModule *module);

#endif
