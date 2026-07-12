#include "meta/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VM_STACK 1024

// Represents a value in the ALIR Virtual Machine memory
typedef struct {
    VarType type;
    union {
        long long int_val;
        double float_val;
        void *ptr_val; // Memory or string pointer
    } as;
} VMValue;

void meta_vm_init(CompilerContext *ctx) {
    // Setup global state for VM if needed
}

// Very basic linear executor for an ALIR Block
static void execute_block(CompilerContext *ctx, AlirBlock *block, VMValue *registers) {
    AlirInst *inst = block->instructions;
    while (inst) {
        switch (inst->op) {
            case ALIR_OP_ADD: {
                if (inst->dest && inst->op1 && inst->op2) {
                    // Extract values from op1 and op2, assuming they map to temp_ids
                    long long v1 = registers[inst->op1->temp_id].as.int_val;
                    long long v2 = registers[inst->op2->temp_id].as.int_val;
                    registers[inst->dest->temp_id].as.int_val = v1 + v2;
                }
                break;
            }
            case ALIR_OP_CALL: {
                if (inst->op1 && inst->op1->kind == ALIR_VAL_VAR) {
                    if (strcmp(inst->op1->val.str_val, "print") == 0) {
                        for (int i = 0; i < inst->arg_count; i++) {
                            AlirValue *arg = inst->args[i];
                            if (arg->kind == ALIR_VAL_CONST) {
                                if (arg->type.base == TYPE_INT) printf("%lld", arg->val.int_val);
                                else if (arg->type.base == TYPE_STRING) printf("%s", arg->val.str_val);
                            } else if (arg->kind == ALIR_VAL_TEMP) {
                                printf("%lld", registers[arg->temp_id].as.int_val);
                            }
                        }
                        printf("\n");
                    }
                }
                break;
            }
            // Add more ALIR opcodes: STORE, LOAD, CONDI, JUMP
            default:
                break;
        }
        inst = inst->next;
    }
}

void meta_vm_execute(CompilerContext *ctx, AlirFunction *func) {
    if (!func) return;
    
    // Virtual Machine Memory Allocation for Virtual Registers
    // ALIR temp registers range from 0 to N.
    // In a complete implementation, this array dynamically resizes.
    VMValue *registers = calloc(MAX_VM_STACK, sizeof(VMValue));
    
    // Standard CFG execution starts at entry block
    AlirBlock *curr_block = func->entry;
    while (curr_block) {
        execute_block(ctx, curr_block, registers);
        
        // Flow resolution (simplified, usually JUMP/CONDI decides next block)
        // For linear meta without branching, next block is just fallback
        curr_block = curr_block->next;
    }
    
    free(registers);
}
