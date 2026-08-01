#ifndef ALKYL_CODEGEN_H
#define ALKYL_CODEGEN_H

#include "alir/alir.h"
#include "common/linker.h"
#include "semantic/semantic.h"
#include "parser/parser.h"

// Run the codegen on the ALIR module to produce output and link it
// Returns 0 on success, non-zero on error.
int backend_run_alir(AlirModule *module, const char *basename, const char *link_flags, int optimization_level, LinkerType linker);
int backend_run_semantic(SemanticCtx *sem_ctx, ASTNode *root, const char *basename, const char *link_flags, int optimization_level, LinkerType linker);

#endif // ALKYL_CODEGEN_H
