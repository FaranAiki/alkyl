#include <stdio.h>
#include <stdlib.h>
#include "include/lexer/lexer.h"
#include "include/common/diagnostic.h"
#include "include/common/context.h"
#include "include/common/common.h"
int main() {
    CompilerContext *ctx = ctx_create();
    Lexer *l = lexer_create(ctx, "project/wmyl/wmyl.kyl");
    Token t;
    int count = 0;
    while ((t = fetch(l)).type != TOKEN_EOF) {
        if (t.line >= 46 && t.line <= 49) {
            printf("line %d, col %d, type %d, text '%s'\n", t.line, t.col, t.type, t.text ? t.text : "");
        }
        if (t.line > 50) break;
    }
    return 0;
}
