#include <stdio.h>
#include "include/lexer/lexer.h"
#include <stdlib.h>
#include <string.h>

int main() {
    Lexer *l = lexer_init();
    l->settings.scope_style = SCOPE_INDENTATION;
    const char *code = "namespace some_ns {\n    int a() {\n        return 0\n    }\n}\n";
    lexer_set_source(l, code, "test");
    Token t;
    do {
        t = lexer_next_token(l);
        printf("Token: %d ('%s') at %d:%d\n", t.type, t.text ? t.text : "", t.line, t.col);
    } while (t.type != TOKEN_EOF && t.type != TOKEN_UNKNOWN);
    return 0;
}
