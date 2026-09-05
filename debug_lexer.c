#include <stdio.h>
#include "src/lexer/lexer.h"
#include "src/common/common.h"
int main() {
    CompilerContext ctx = {0};
    Lexer l;
    lexer_init(&l, &ctx, "link \"wayland-server\";", "test");
    Token t;
    while(lexer_next_token(&l, &t)) {
        printf("Token: %d\n", t.type);
    }
    return 0;
}
