#ifndef META_EVAL_H
#define META_EVAL_H

#include "parser/typestruct.h"
#include "common/context.h"

// Values returned by the interpreter during compile-time evaluation.
typedef enum {
    VAL_NULL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL
} MetaValType;

typedef struct {
    MetaValType type;
    union {
        long long int_val;
        double float_val;
        char *str_val;
        bool bool_val;
    } as;
} MetaValue;

// Initialize the interpreter context.
void meta_init(CompilerContext *ctx);

// Evaluate an AST expression directly and return its value.
MetaValue meta_eval_expr(ASTNode *expr);

// Execute an AST statement (or block) directly.
void meta_eval_stmt(ASTNode *stmt);

#endif // META_EVAL_H
