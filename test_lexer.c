#include <stdio.h>
#include <stdlib.h>
#include "include/lexer/lexer.h"
#include "include/common/context.h"
#include "include/parser/parser.h"

int main() {
    CompilerContext *ctx = compiler_context_create();
    FILE *f = fopen("project/wmyl/wmyl.kyl", "r");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *code = malloc(fsize + 1);
    fread(code, 1, fsize, f);
    code[fsize] = 0;
    fclose(f);

    Lexer l;
    lexer_init(&l, ctx, "project/wmyl/wmyl.kyl", code, NULL);
    l.settings.scope_style = SCOPE_INDENTATION; // Wait, parser_create DOES NOT SET THIS?! Let me try with default first!
    while(1) {
        Token t = lexer_next(&l);
        if (t.line >= 332 && t.line <= 335) {
            printf("token type=%d text=%s line=%d\n", t.type, t.text?t.text:"");
        }
        if (t.type == TOKEN_EOF) break;
    }
    return 0;
}
