#ifndef PARSER_H
#define PARSER_H

#include "../lexer/lexer.h"
#include "../common/debug.h"
#include "../common/context.h"
#include <setjmp.h>
#include <stdbool.h>

#include "parser/typestruct.h"
#include "semantic/typestruct.h"

typedef struct Macro Macro;
typedef struct TypeName TypeName;
typedef struct TypeAlias TypeAlias;
typedef struct Expansion Expansion;

typedef struct {
    int require_parens_for_conditions;
    int allow_implicit_return;
    int allow_postfix_types;       // e.g., let x: int
    int strict_boolean_conditions; // e.g., if (1) vs if (true)
} ParserSettings;

typedef struct Parser {
    Lexer *l;
    CompilerContext *ctx;
    ParserSettings settings;
    
    Token current_token;
    jmp_buf *recover_buf;
    
    Macro *macro_head;
    TypeName *type_head;
    TypeAlias *alias_head;
    struct Expansion *expansion_head;
    int disable_macro_expansion;
} Parser;

void parser_init(Parser *p, Lexer *l, ParserSettings *settings);

Token parser_peek_token(Parser *p);
void parser_prescan(Parser *p);

ASTNode* parse_program(Parser *p);
ASTNode* parse_expression(Parser *p);

#include "emitter.h"
#include "link.h"
#include "modifier.h"

#endif
