#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/lexer/c_lexer.c"

int main() {
    CompilerContext ctx = {0};
    CLexer l = {0};
    c_lexer_init(&l, &ctx, "test", "int __base) __asm__ (\"\" \"__isoc23_strtoimax\") __attribute__ ((__nothrow__ , __leaf__)) ;");
    
    CToken t;
    do {
        t = c_lexer_next(&l);
        printf("type=%d text='%s'\n", t.type, t.text ? t.text : "NULL");
    } while (t.type != C_TOKEN_EOF);
    return 0;
}
