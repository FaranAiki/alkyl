/**
 * @file vm.h
 * @brief Metalir virtual machine interface.
 */
#ifndef METALIR_VM_H
#define METALIR_VM_H

#include "alir/alir.h"
#include "common/arena.h"

/**
 * @brief A global variable in the Metalir VM.
 */
typedef struct VMGlobal {
    char *name;
    void *ptr_val;
    struct VMGlobal *next;
} VMGlobal;

/**
 * @brief The Metalir virtual machine.
 */
typedef struct MetalirVM {
    Arena *arena;
    void *registers;
    VMGlobal *globals;
    int status;
} MetalirVM;

/**
 * @brief Initializes a Metalir VM.
 * @param arena The arena allocator.
 * @return The initialized VM.
 */
MetalirVM* metalir_vm_init(Arena *arena);

/**
 * @brief Frees a Metalir VM.
 * @param vm The VM to free.
 */
void metalir_vm_free(MetalirVM *vm);

/**
 * @brief Executes a function in the VM.
 * @param vm The VM.
 * @param module The ALIR module.
 * @param func The function to execute.
 * @param sem_ctx_ptr Semantic context pointer.
 * @param args Argument array.
 * @param arg_count Number of arguments.
 * @return The return value.
 */
long long metalir_vm_execute(MetalirVM *vm, struct AlirModule *module, struct AlirFunction *func, void *sem_ctx_ptr, long long *args, int arg_count);

/**
 * @brief Resolves an ALIR value to a VM value.
 * @param val The ALIR value.
 * @param module The ALIR module.
 * @param vm The VM.
 * @param args Argument array.
 * @param arg_count Number of arguments.
 * @return The resolved value.
 */
long long metalir_vm_resolve_var(AlirValue *val, AlirModule *module, MetalirVM *vm, long long *args, int arg_count);

#endif // METALIR_VM_H
