#ifndef ALKYL_PARSER_FRAGMENT_CLASS_H
#define ALKYL_PARSER_FRAGMENT_CLASS_H

#include "parser/typestruct.h"
#include "parser/parser.h"

ASTNode* parse_class(Parser *p);
ASTNode* parse_class_impl(Parser *p, int modifiers);
ASTNode* parse_enum(Parser *p);

#endif // ALKYL_PARSER_FRAGMENT_CLASS_H
