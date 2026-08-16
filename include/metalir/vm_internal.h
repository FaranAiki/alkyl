/**
 * @file vm_internal.h
 * @brief Metalir virtual machine internal declarations.
 */
#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include "vm.h"
#include "alir/alir.h"
#include "semantic/semantic.h"

#define MAX_VM_STACK 1024

/**
 * @brief A value in the Metalir VM.
 */
typedef struct {
    union {
        long long int_val;
        double single_val;
        void *ptr_val;
    } as;
} VMValue;

/**
 * @brief The execution context for the Metalir VM.
 */
typedef struct {
    MetalirVM *vm;
    AlirModule *module;
    AlirFunction *func;
    SemanticCtx *sem_ctx;
    long long *args;
    int arg_count;

    VMValue *registers;
    AlirBlock **next_block;
    long long *ret_val;
    int should_return;
} VMContext;

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

/**
 * @brief Evaluates a memory instruction.
 * @param ctx The VM context.
 * @param inst The ALIR instruction.
 */
void vm_eval_mem(VMContext *ctx, AlirInst *inst);

/**
 * @brief Evaluates a math instruction.
 * @param ctx The VM context.
 * @param inst The ALIR instruction.
 */
void vm_eval_math(VMContext *ctx, AlirInst *inst);

/**
 * @brief Evaluates a call instruction.
 * @param ctx The VM context.
 * @param inst The ALIR instruction.
 */
void vm_eval_call(VMContext *ctx, AlirInst *inst);

/**
 * @brief Evaluates a flow control instruction.
 * @param ctx The VM context.
 * @param inst The ALIR instruction.
 */
void vm_eval_flow(VMContext *ctx, AlirInst *inst);

/**
 * @brief Evaluates a miscellaneous instruction.
 * @param ctx The VM context.
 * @param inst The ALIR instruction.
 */
void vm_eval_misc(VMContext *ctx, AlirInst *inst);

/**
 * @brief Finds a basic block by label.
 * @param func The ALIR function.
 * @param label The block label.
 * @return The block, or NULL if not found.
 */
AlirBlock* find_block(AlirFunction *func, const char *label);

#endif // VM_INTERNAL_H
