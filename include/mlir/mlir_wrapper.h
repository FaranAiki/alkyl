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
AlkylMlirValue alkyl_mlir_build_shl(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);
AlkylMlirValue alkyl_mlir_build_shr(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

// Control Flow (Mocked for now)
void alkyl_mlir_build_scf_if(AlkylMlirContext ctx, AlkylMlirValue cond);
void alkyl_mlir_build_scf_while(AlkylMlirContext ctx, AlkylMlirValue cond);

#ifdef __cplusplus
}
#endif

#endif // ALKYL_MLIR_WRAPPER_H
