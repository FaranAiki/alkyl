#include <stdio.h>
#include "include/lexer/lexer.h"
int main() {
    printf("EOF: %d\n", TOKEN_EOF);
    printf("LBRACE: %d\n", TOKEN_LBRACE);
    printf("IDENTIFIER: %d\n", TOKEN_IDENTIFIER);
    return 0;
}
