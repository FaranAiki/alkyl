#include <stdio.h>
#include <stdlib.h>
#include "include/parser/parser.h"

int main() {
    CompilerContext ctx = {0};
    SemanticSettings sem = {0};
    MetalirRunner *r = metalir_runner_create("test", &sem, 1);
    
    const char *src = "class Server {\n    void* new_input;\n}\n";
    ASTNode *root = metalir_parse(r, src, "test.kyl", NULL);
    
    if (r->parser.has_error) {
        printf("Parse failed\n");
    } else {
        printf("Parse OK\n");
    }
    return 0;
}
