#ifndef SEMANTIC_EMITTER_H
#define SEMANTIC_EMITTER_H

#include "semantic.h"

// Dump the symbol table and scope hierarchy to string
char* semantic_to_string(SemanticCtx *ctx);

// Dump the symbol table and scope hierarchy to a file
void semantic_to_file(SemanticCtx *ctx, const char *filename);

#endif // SEMANTIC_EMITTER_H
