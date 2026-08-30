#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/lexer/lexer.h"
#include "include/common/context.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    CompilerContext *ctx = compiler_context_create();
    
    char *code = malloc(100000);
    FILE *f = fopen(argv[1], "r");
    fread(code, 1, 100000, f);
    fclose(f);
    
    Lexer l;
    lexer_init(&l, ctx, argv[1], code, NULL);
    
    while(1) {
        Token t = lexer_next(&l);
        if (t.type == TOKEN_EOF) break;
        if (t.line >= 332 && t.line <= 335) {
            printf("Line %d:%d Type: %d Text: '%s'\n", t.line, t.col, t.type, t.text ? t.text : "");
        }
    }
    return 0;
}
