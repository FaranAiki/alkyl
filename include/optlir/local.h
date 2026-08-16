/**
 * @file local.h
 * @brief Local ALIR optimization declarations.
 */
#ifndef OPTLIR_LOCAL_H
#define OPTLIR_LOCAL_H

#include "../alir/alir.h"

/**
 * @brief Runs local optimizations on the module.
 * @param module The ALIR module.
 */
void optlir_local_optimize(AlirModule *module);

#endif
