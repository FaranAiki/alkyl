#ifndef PARSER_MODIFIER_H
#define PARSER_MODIFIER_H

#include "parser.h"

ASTNode* parse_wash_or_clean_tail(Parser *p, char *var_name, int wash_type);

#endif // PARSER_MODIFIER_H
