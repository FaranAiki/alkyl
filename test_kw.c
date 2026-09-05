#include <stdio.h>
#include "include/lexer/lexer.h"
int main() {
    printf("TOKEN_EXPORT = %d\n", TOKEN_EXPORT);
    int found = 0;
    int num = sizeof(keywords)/sizeof(keywords[0]);
    for(int i=0; i<num; i++) {
        if(keywords[i].word && strcmp(keywords[i].word, "export") == 0) found = 1;
    }
    printf("export found = %d\n", found);
    return 0;
}
