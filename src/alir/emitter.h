#ifndef ALIR_EMITTER_H
#define ALIR_EMITTER_H
#include "alir.h"

void alir_emit_stream(AlirModule *mod, FILE *f);
void alir_print(AlirModule *mod);
void alir_emit_to_file(AlirModule *mod, const char *filename);
AlirModule* alir_generate(SemanticCtx *sem, ASTNode *root);

void alir_fprint_type(FILE *f, VarType t);
void alir_fprint_val(FILE *f, AlirValue *v);

#endif // ALIR_EMITTER_H
