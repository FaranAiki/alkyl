#ifndef ALKYL_PARSER_FRAGMENT_LOOP_H
#define ALKYL_PARSER_FRAGMENT_LOOP_H

#include "parser/typestruct.h"
#include "parser/parser.h"

ASTNode* parse_while(Parser *p);
ASTNode* parse_loop(Parser *p);

#endif // ALKYL_PARSER_FRAGMENT_LOOP_H
