/**
 * @file translate.h
 * @brief ALIR-to-LLVM translation declarations.
 */
#ifndef LLVM_CODEGEN_TRANSLATE_H
#define LLVM_CODEGEN_TRANSLATE_H

#include "codegen.h"

/**
 * @brief Translates an ALIR instruction to LLVM IR.
 * @param ctx The code generation context.
 * @param inst The ALIR instruction.
 */
void translate_inst(CodegenCtx *ctx, AlirInst *inst);

#endif // LLVM_CODEGEN_TRANSLATE_H
