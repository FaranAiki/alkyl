/**
 * @file alick_internal.h
 * @brief Internal declarations for the ALIR checker.
 */
#ifndef ALICK_INTERNAL_H
#define ALICK_INTERNAL_H

#include "alick.h"
#include <stdio.h>

/**
 * @brief Logs an error in the ALIR checker.
 * @param ctx The checker context.
 * @param func The ALIR function.
 * @param block The ALIR block.
 * @param inst The ALIR instruction.
 * @param fmt The printf-style format string.
 * @param ... Format arguments.
 */
void alick_error(AlickCtx *ctx, AlirFunction *func, AlirBlock *block, AlirInst *inst, const char *fmt, ...);

/**
 * @brief Logs a warning in the ALIR checker.
 * @param ctx The checker context.
 * @param func The ALIR function.
 * @param block The ALIR block.
 * @param inst The ALIR instruction.
 * @param fmt The printf-style format string.
 * @param ... Format arguments.
 */
void alick_warning(AlickCtx *ctx, AlirFunction *func, AlirBlock *block, AlirInst *inst, const char *fmt, ...);

/**
 * @brief Pass 1: Control Flow Graph validation.
 * @param ctx The checker context.
 * @param func The ALIR function.
 */
void alick_check_cfg(AlickCtx *ctx, AlirFunction *func);

/**
 * @brief Pass 2: Type, operand, and structural validation.
 * @param ctx The checker context.
 * @param func The ALIR function.
 */
void alick_check_types(AlickCtx *ctx, AlirFunction *func);

/**
 * @brief Pass 3: Localized memory validity (dangling pointers, UAF, double-free).
 * @param ctx The checker context.
 * @param func The ALIR function.
 */
void alick_check_memory(AlickCtx *ctx, AlirFunction *func);

#endif // ALICK_INTERNAL_H
