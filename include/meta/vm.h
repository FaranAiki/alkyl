#ifndef META_VM_H
#define META_VM_H

#include "alir/alir.h"
#include "common/context.h"

// Initialize the ALIR Virtual Machine for meta execution.
void meta_vm_init(CompilerContext *ctx);

// Execute an ALIR function at compile-time (used for meta and postmeta blocks).
void meta_vm_execute(CompilerContext *ctx, AlirFunction *func);

#endif // META_VM_H
