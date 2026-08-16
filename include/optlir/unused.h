/**
 * @file unused.h
 * @brief Unused-code elimination declarations for ALIR.
 */
#ifndef OPTLIR_UNUSED_H
#define OPTLIR_UNUSED_H

#include "../alir/alir.h"

/**
 * @brief Removes unused functions and globals from the module.
 * @param module The ALIR module.
 */
void optlir_remove_unused(AlirModule *module);

#endif
