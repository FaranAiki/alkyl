#ifndef ALKYL_CODEGEN_H
#define ALKYL_CODEGEN_H

#include "alir/alir.h"

// Run the codegen on the ALIR module to produce output and link it
// Returns 0 on success, non-zero on error.
int backend_run(AlirModule *module, const char *basename, const char *link_flags);

#endif // ALKYL_CODEGEN_H
