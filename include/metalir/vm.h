#ifndef METALIR_VM_H
#define METALIR_VM_H

#include "alir/alir.h"
#include "common/arena.h"

typedef struct VMGlobal {
    char *name;
    void *ptr_val; 
    struct VMGlobal *next;
} VMGlobal;

typedef struct MetalirVM {
    Arena *arena;
    void *registers;
    VMGlobal *globals;
    int status;
} MetalirVM;

MetalirVM* metalir_vm_init(Arena *arena);
void metalir_vm_free(MetalirVM *vm);
long long metalir_vm_execute(MetalirVM *vm, struct AlirModule *module, struct AlirFunction *func, void *sem_ctx_ptr, long long *args, int arg_count);
long long metalir_vm_resolve_var(AlirValue *val, AlirModule *module, MetalirVM *vm, long long *args, int arg_count);

#endif
