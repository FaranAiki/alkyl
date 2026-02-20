#ifndef PARSER_MODIFIER_H
#define PARSER_MODIFIER_H

#include "parser.h"

ASTNode* parse_wash_or_clean_tail(Parser *p, ASTNode *expr, int wash_type);

#endif // PARSRE_MODIFIER_H
