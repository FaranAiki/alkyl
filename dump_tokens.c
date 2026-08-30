#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/lexer/lexer.h"
#include "include/common/context.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    CompilerContext *ctx = compiler_context_create();
    Lexer *l = lexer_create(ctx, argv[1]);
    if (!l) return 1;
    
    int inside_server = 0;
    while(1) {
        Token t = fetch_token(l);
        if (t.type == TOKEN_EOF) break;
        
        if (t.type == TOKEN_IDENTIFIER && t.text && strcmp(t.text, "Server") == 0) {
            inside_server = 1;
        }
        
        if (inside_server) {
            printf("Token: type=%d, text='%s'\n", t.type, t.text ? t.text : "");
        }
    }
    return 0;
}
