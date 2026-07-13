#ifndef ALKYL_PARSER_FRAGMENT_COND_H
#define ALKYL_PARSER_FRAGMENT_COND_H

#include "parser/typestruct.h"
#include "parser/parser.h"

ASTNode* parse_if(Parser *p);
ASTNode* parse_switch(Parser *p);

#endif // ALKYL_PARSER_FRAGMENT_COND_H
