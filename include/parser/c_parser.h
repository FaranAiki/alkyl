#ifndef C_PARSER_H
#define C_PARSER_H

#include <stdbool.h>
#include "typestruct.h"
#include "../common/context.h"
#include "../common/hashmap.h"
#include "c_lexer.h"

typedef struct CParser CParser;

struct CParser {
    CLexer lexer;
    CToken current;
    CompilerContext *ctx;
    int has_error;

    // Typedef resolution table
    HashMap typedef_map;
    struct {
        char **names;
        VarType *types;
        int count;
        int capacity;
    } typedefs;

    // Macro definitions from #define
    struct {
        char **names;
        char **values;
        int count;
        int capacity;
    } defines;

    // Conditional compilation stack
    struct {
        int *active;
        int count;
        int capacity;
    } cond_stack;
};

void c_parser_init(CParser *p, CompilerContext *ctx, const char *filename, const char *source);
char* c_preprocess_header(CompilerContext *ctx, const char *fname);
ASTNode* c_parse_header(CParser *p);

#endif
