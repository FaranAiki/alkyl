/**
 * @file codegen.h
 * @brief Backend codegen runner interface.
 */
#ifndef ALKYL_CODEGEN_H
#define ALKYL_CODEGEN_H

#include "alir/alir.h"
#include "common/linker.h"
#include "semantic/semantic.h"
#include "parser/parser.h"

/**
 * @brief Run the codegen pipeline on an ALIR module to produce output and link it.
 * @param module ALIR module to compile.
 * @param basename Base name for output files (e.g., "out" produces out.o).
 * @param link_flags Additional flags passed to the system linker.
 * @param optimization_level Optimization level (0 = unopt, >0 = opt).
 * @param linker Type of linker to use (LINKER_CC, LINKER_LLVM, etc.).
 * @return 0 on success, non-zero on error.
 */
int backend_run_alir(AlirModule *module, const char *basename, const char *link_flags, int optimization_level, LinkerType linker);

/**
 * @brief Run the full compilation pipeline from semantic analysis through linking.
 * @param sem_ctx Semantic context (type-checked AST).
 * @param root Root AST node of the program.
 * @param basename Base name for output files.
 * @param link_flags Additional flags passed to the system linker.
 * @param optimization_level Optimization level (0 = unopt, >0 = opt).
 * @param linker Type of linker to use.
 * @return 0 on success, non-zero on error.
 */
int backend_run_semantic(SemanticCtx *sem_ctx, ASTNode *root, const char *basename, const char *link_flags, int optimization_level, LinkerType linker);

#endif // ALKYL_CODEGEN_H
