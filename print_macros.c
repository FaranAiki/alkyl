#include <stdio.h>
#include "include/parser/parser.h"

int main() {
    CompilerContext *ctx = compiler_context_create();
    Parser *p = parser_create(ctx, "project/wmyl/wmyl.kyl", "");
    // ... we don't have the full environment to easily run this, nvm.
}
