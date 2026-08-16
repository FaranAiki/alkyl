/**
 * @file flux.h
 * @brief Flux-specific LLVM codegen helpers.
 */
#ifndef LLVM_CODEGEN_FRAGMENT_FLUX_H
#define LLVM_CODEGEN_FRAGMENT_FLUX_H

#include "../codegen.h"

/**
 * @brief Gets a flux iterator value.
 * @param ctx The code generation context.
 * @param inst The ALIR instruction.
 * @param op1 The first operand.
 * @param res Output parameter for the result.
 */
void codegen_llvm_flux_iter_get(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef *res);

/**
 * @brief Advances a flux iterator.
 * @param ctx The code generation context.
 * @param op1 The iterator value.
 */
void codegen_llvm_flux_iter_next(CodegenCtx *ctx, LLVMValueRef op1);

/**
 * @brief Checks if a flux iterator is valid.
 * @param ctx The code generation context.
 * @param op1 The iterator value.
 * @param res Output parameter for the result.
 */
void codegen_llvm_flux_iter_valid(CodegenCtx *ctx, LLVMValueRef op1, LLVMValueRef *res);

/**
 * @brief Initializes a flux iterator.
 * @param ctx The code generation context.
 * @param inst The ALIR instruction.
 * @param op1 The first operand.
 * @param res Output parameter for the result.
 */
void codegen_llvm_flux_iter_init(CodegenCtx *ctx, AlirInst *inst, LLVMValueRef op1, LLVMValueRef *res);

#endif // LLVM_CODEGEN_FRAGMENT_FLUX_H
