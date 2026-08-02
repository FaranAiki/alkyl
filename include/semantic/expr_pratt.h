#ifndef PARSER_EXPR_PRATT_H
#define PARSER_EXPR_PRATT_H
#include "parser_internal.h"

// TODO make it so that we can actually add other precedence
typedef enum {
    PREC_NONE,
    PREC_ASSIGN,      // =, +=, -=, etc.
    PREC_FALLBACK,    // ?
    PREC_OR,          // ||
    PREC_AND,         // &&
    PREC_BIT_OR,      // |
    PREC_BIT_XOR,     // ^
    PREC_BIT_AND,     // &
    PREC_EQUALITY,    // ==, !=
    PREC_COMPARISON,  // <, >, <=, >=
    PREC_SHIFT,       // <<, >>
    PREC_TERM,        // +, -
    PREC_FACTOR,      // *, /, %
    PREC_UNARY,       // !, -, ~, typeof, sizeof, ++, --
    PREC_CALL,        // ., (), [], ++, -- (postfix)
    PREC_PRIMARY
} Precedence;

typedef ASTNode* (*ParsePrefixFn)(Parser *p);
typedef ASTNode* (*ParseInfixFn)(Parser *p, ASTNode *left);

typedef struct {
    ParsePrefixFn prefix;
    ParseInfixFn infix;
    Precedence precedence;
} ParseRule;

#endif // PARSER_EXPR_PRATT_H
