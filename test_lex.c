#include <stdio.h>
#include <stdlib.h>
#include "include/lexer/lexer.h"
#include "include/common/context.h"

int main() {
    CompilerContext ctx = {0};
    LexerSettings settings = {0};
    Lexer l = {0};
    
    FILE *f = fopen("project/wmyl/wmyl.kyl", "r");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *src = malloc(fsize + 1);
    fread(src, fsize, 1, f);
    src[fsize] = 0;
    fclose(f);
    
    lexer_init(&l, &ctx, "wmyl.kyl", src, &settings);
    
    Token t;
    do {
        t = lexer_next(&l);
        printf("%d:%d %d '%s'\n", t.line, t.col, t.type, t.text ? t.text : "");
        if (t.line > 15) break;
    } while (t.type != TOKEN_EOF);
    
    return 0;
}
