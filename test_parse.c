#include "include/parser/parser.h"
#include "include/lexer/lexer.h"
#include "include/metalir/metalir.h"
#include <stdio.h>

int main() {
    CompilerContext ctx = {0};
    MetalirRunner r = {0};
    r.ctx = ctx;
    const char *buffer = "namespace some_ns {\n    int a() {\n        return 0\n    }\n}";
    ASTNode *root = metalir_parse(&r, buffer, "test", NULL);
    if (!root) {
        printf("Parse failed\n");
        return 1;
    }
    ASTNode *curr = root;
    while(curr) {
        printf("Node type: %d\n", curr->type);
        if (curr->type == NODE_NAMESPACE) {
            NamespaceNode *ns = (NamespaceNode*)curr;
            ASTNode *body = ns->body;
            while(body) {
                printf("  Body node type: %d\n", body->type);
                body = body->next;
            }
        }
        curr = curr->next;
    }
    return 0;
}
