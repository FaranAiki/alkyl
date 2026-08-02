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

// Entrypoint
void mlir_generate(ASTNode *root, const char *basename);

// Core Generation Functions
void mlir_gen_stmt(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node);
AlkylMlirValue mlir_gen_expr(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node);
void mlir_gen_type(AlkylMlirContext ctx, VarType type);

void reset_mlir_defers();
void execute_mlir_defers(AlkylMlirContext ctx, AlkylMlirModule mod);

#endif // MLIR_GENERATE_H
