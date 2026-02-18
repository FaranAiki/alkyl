#ifndef SEMANTIC_EMITTER_H
#define SEMANTIC_EMITTER_H

#include "semantic.h"
#include "../common/common.h"

// TODO copy 

// idk why u should need static lol
void semantic_emit_scope(StringBuilder *sb, SemScope *scope, int indent);
void semantic_emit_symbol(StringBuilder *sb, SemSymbol *sym, int indent);
void semantic_emit_type_str(StringBuilder *sb, VarType t);
void semantic_emit_indent(StringBuilder *sb, int indent);

// Dump the symbol table and scope hierarchy to string
char* semantic_to_string(SemanticCtx *ctx);

// Dump the symbol table and scope hierarchy to a file
void semantic_to_file(SemanticCtx *ctx, const char *filename);

#endif // SEMANTIC_EMITTER_H
