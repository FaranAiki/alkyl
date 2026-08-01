#ifndef ALKYL_MLIR_WRAPPER_H
#define ALKYL_MLIR_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* AlkylMlirContext;
typedef void* AlkylMlirModule;
typedef void* AlkylMlirFunc;
typedef void* AlkylMlirBlock;
typedef void* AlkylMlirValue;

AlkylMlirContext alkyl_mlir_create_context();
AlkylMlirModule alkyl_mlir_create_module(AlkylMlirContext ctx, const char* name);
void alkyl_mlir_destroy_context(AlkylMlirContext ctx);

// Builder wrappers
AlkylMlirFunc alkyl_mlir_add_function(AlkylMlirContext ctx, AlkylMlirModule mod, const char* name);
AlkylMlirBlock alkyl_mlir_add_block(AlkylMlirFunc func);
void alkyl_mlir_set_insertion_point_to_end(AlkylMlirContext ctx, AlkylMlirBlock block);

// Statements
void alkyl_mlir_build_return(AlkylMlirContext ctx, AlkylMlirValue val);
AlkylMlirValue alkyl_mlir_build_alloca(AlkylMlirContext ctx, const char* name);
void alkyl_mlir_build_store(AlkylMlirContext ctx, AlkylMlirValue val, AlkylMlirValue ptr);

// Expressions
AlkylMlirValue alkyl_mlir_build_int_constant(AlkylMlirContext ctx, int val);
AlkylMlirValue alkyl_mlir_build_add(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_sub(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_mul(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_div(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_mod(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_shl(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_shr(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_and(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_or(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_xor(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

AlkylMlirValue alkyl_mlir_build_load(AlkylMlirContext ctx, AlkylMlirValue ptr);
AlkylMlirValue alkyl_mlir_build_call(AlkylMlirContext ctx, const char* name, AlkylMlirValue* args, int num_args);

// Control Flow
void* alkyl_mlir_build_scf_if_start(AlkylMlirContext ctx, AlkylMlirValue cond, int has_else);
void alkyl_mlir_build_scf_if_else(AlkylMlirContext ctx, void* if_op_ptr);
void alkyl_mlir_build_scf_if_end(AlkylMlirContext ctx, void* if_op_ptr);

void alkyl_mlir_build_scf_while(AlkylMlirContext ctx, AlkylMlirValue cond);

void* alkyl_mlir_build_switch_start(AlkylMlirContext ctx, AlkylMlirValue cond, int num_cases);
void alkyl_mlir_build_switch_case_start(AlkylMlirContext ctx, void* switch_op_ptr, AlkylMlirValue val, int is_leak);
void alkyl_mlir_build_switch_case_end(AlkylMlirContext ctx, void* switch_op_ptr);
void alkyl_mlir_build_switch_default_start(AlkylMlirContext ctx, void* switch_op_ptr);
void alkyl_mlir_build_switch_default_end(AlkylMlirContext ctx, void* switch_op_ptr);
void alkyl_mlir_build_switch_end(AlkylMlirContext ctx, void* switch_op_ptr);

#ifdef __cplusplus
}
#endif

#endif // ALKYL_MLIR_WRAPPER_H
