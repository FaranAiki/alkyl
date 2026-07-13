#ifndef PARSER_MODIFIER_H
#define PARSER_MODIFIER_H

#include "parser.h"

typedef enum {
    WASH_TYPE_WASH = 0,
    WASH_TYPE_CLEAN = 1,
    WASH_TYPE_UNTAINT = 2
} WashType;

ASTNode* parse_wash_or_clean_tail(Parser *p, ASTNode *target, WashType wash_type);

#endif // PARSER_MODIFIER_H
