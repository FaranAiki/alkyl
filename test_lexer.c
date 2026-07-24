#include "src/lexer/lexer.h"
#include "src/common/arena.h"
#include <stdio.h>

int main() {
    Lexer l;
    LexerSettings settings = {0};
    settings.scope_style = SCOPE_INDENTATION;
    settings.require_semicolons = 0;
    settings.spaces_per_indent = 4;
    
    char* src = "for t in [1, 2]\n    123\n\n";
    CompilerContext ctx = {0};
    lexer_init(&l, &ctx, "test", src, &settings);
    
    Token t;
    do {
        t = lex(&l);
        printf("Type: %d, Text: %s\n", t.type, t.text ? t.text : "null");
    } while (t.type != TOKEN_EOF);
    return 0;
}
