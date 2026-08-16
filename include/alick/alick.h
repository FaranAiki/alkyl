/**
 * @file alick.h
 * @brief ALIR checker (Alick) interface.
 */
#ifndef ALICK_H
#define ALICK_H

#include "../alir/alir.h"

// Context for tracking errors during ALIR checking
/**
 * @brief The ALIR checker context.
 */
typedef struct AlickCtx {
    AlirModule *module;
    int error_count;
    int warning_count;
} AlickCtx;

/**
 * @brief Checks an ALIR module for errors.
 * @param mod The ALIR module.
 * @return The number of errors found.
 */
int alick_check_module(AlirModule *mod);

#endif // ALICK_H
