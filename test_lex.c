#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/common/common.h"
#include "include/lexer/c_lexer.h"
#include "include/parser/c_parser.h"

// Define stub for c_lexer.c dependencies
void diag_err(CompilerContext *ctx, int line, int col, const char *msg) {}
void diag_warn(CompilerContext *ctx, int line, int col, const char *msg) {}
void diag_note(CompilerContext *ctx, int line, int col, const char *msg) {}

int main() {
    CompilerContext ctx = {0};
    CParser p = {0};
    c_parser_init(&p, &ctx, "test", "int __base) __asm__ (\"\" \"__isoc23_strtoimax\")");
    
    CToken t;
    do {
        t = c_lexer_next(&p.lexer);
        printf("type=%d text='%s'\n", t.type, t.text ? t.text : "NULL");
    } while (t.type != C_TOKEN_EOF);
    return 0;
}
