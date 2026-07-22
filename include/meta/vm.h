#ifndef META_VM_H
#define META_VM_H

#include "alir/alir.h"

typedef struct VMGlobal {
    char *name;
    void *ptr_val; 
    struct VMGlobal *next;
} VMGlobal;

// VM State context
typedef struct MetaVM {
    void *registers; // Pointer to VMValue array
    VMGlobal *globals;
    int status;
} MetaVM;

// Initialize the ALIR Virtual Machine context
MetaVM* meta_vm_init();

// Clean up the VM context
void meta_vm_free(MetaVM *vm);

// Execute an ALIR function at compile-time (used for meta and postmeta blocks).
// Returns 0 on success, non-zero on error.
long long meta_vm_execute(MetaVM *vm, AlirModule *module, AlirFunction *func, void *sem_ctx);

#endif // META_VM_H
