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

#endif // MLIR_GENERATE_H
