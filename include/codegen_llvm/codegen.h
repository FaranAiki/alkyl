/**
 * @file codegen.h
 * @brief LLVM code generation interface for ALIR.
 */
#ifndef LLVM_CODEGEN_H
#define LLVM_CODEGEN_H

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include "../../include/alir/alir.h"

/**
 * @brief The LLVM code generation context.
 */
typedef struct CodegenCtx {
    AlirModule *alir_mod;
    LLVMContextRef llvm_ctx;
    LLVMModuleRef llvm_mod;
    LLVMBuilderRef builder;

    HashMap value_map;      // Maps: Name -> LLVMValueRef (For locals/params)
    LLVMValueRef *temps;    // Maps: temp_id -> LLVMValueRef
    int max_temps;

    HashMap block_map;      // Maps: Label -> LLVMBasicBlockRef
    HashMap struct_map;     // Maps: Class/Struct Name -> LLVMTypeRef
    HashMap func_map;       // Maps: Function Name -> LLVMValueRef
    HashMap func_type_map;  // Maps: Function Name -> LLVMTypeRef

    Arena *arena;           // Borrowed from compiler context
} CodegenCtx;

/**
 * @brief Initializes the code generator for an ALIR module.
 * @param mod The ALIR module.
 * @return The code generation context.
 */
CodegenCtx* codegen_init(AlirModule *mod);

/**
 * @brief Generates the LLVM module.
 * @param ctx The code generation context.
 * @return The generated LLVM module.
 */
LLVMModuleRef codegen_generate(CodegenCtx *ctx);

/**
 * @brief Emits the LLVM IR to a file.
 * @param ctx The code generation context.
 * @param filename The output filename.
 */
void codegen_emit_to_file(CodegenCtx *ctx, const char *filename);

/**
 * @brief Prints the LLVM IR to stdout.
 * @param ctx The code generation context.
 */
void codegen_print(CodegenCtx *ctx);

/**
 * @brief Disposes the code generation context.
 * @param ctx The code generation context.
 */
void codegen_dispose(CodegenCtx *ctx);

/**
 * @brief Sets the LLVM value for an ALIR value.
 * @param ctx The code generation context.
 * @param v The ALIR value.
 * @param llvm_val The LLVM value.
 */
void set_llvm_value(CodegenCtx *ctx, AlirValue *v, LLVMValueRef llvm_val);

/**
 * @brief Gets the LLVM type for an Alkyl type.
 * @param ctx The code generation context.
 * @param t The Alkyl type.
 * @return The LLVM type.
 */
LLVMTypeRef get_llvm_type(CodegenCtx *ctx, VarType t);

/**
 * @brief Gets the LLVM value for an ALIR value.
 * @param ctx The code generation context.
 * @param v The ALIR value.
 * @return The LLVM value.
 */
LLVMValueRef get_llvm_value(CodegenCtx *ctx, AlirValue *v);

#include "translate.h"
#include "fragment/flux.h"

#endif // LLVM_CODEGEN_H
