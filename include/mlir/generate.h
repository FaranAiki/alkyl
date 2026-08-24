/**
 * @file generate.h
 * @brief MLIR generation declarations.
 */
#ifndef MLIR_GENERATE_H
#define MLIR_GENERATE_H

#include <stdbool.h>
#include "common/context.h"
#include <stdbool.h>
#include "common/context.h"
#include "lexer/lexer.h"
#include "parser/typestruct.h"
#include "semantic/typestruct.h"
#include "semantic/check.h"
#include "mlir/mlir_wrapper.h"

/**
 * @brief Entrypoint for MLIR generation.
 * @param root The root AST node.
 * @param basename The base name for output files.
 */
void mlir_generate(ASTNode *root, const char *basename);

/**
 * @brief Generates MLIR for a statement.
 * @param ctx The MLIR context.
 * @param mod The MLIR module.
 * @param node The statement AST node.
 */
void mlir_gen_stmt(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node);

/**
 * @brief Generates MLIR for an expression.
 * @param ctx The MLIR context.
 * @param mod The MLIR module.
 * @param node The expression AST node.
 * @return The generated MLIR value.
 */
AlkylMlirValue mlir_gen_expr(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node);

/**
 * @brief Resets the MLIR defer stack.
 */
void reset_mlir_defers(void);

/**
 * @brief Executes pending MLIR defer statements.
 * @param ctx The MLIR context.
 * @param mod The MLIR module.
 */
void execute_mlir_defers(AlkylMlirContext ctx, AlkylMlirModule mod);

#endif // MLIR_GENERATE_H
