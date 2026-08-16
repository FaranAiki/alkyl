/**
 * @file mlir_wrapper.h
 * @brief MLIR C API wrapper for the Alkyl compiler.
 */
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

/**
 * @brief Creates an MLIR context.
 * @return The new context.
 */
AlkylMlirContext alkyl_mlir_create_context();

/**
 * @brief Creates an MLIR module.
 * @param ctx The MLIR context.
 * @param name The module name.
 * @return The new module.
 */
AlkylMlirModule alkyl_mlir_create_module(AlkylMlirContext ctx, const char* name);

/**
 * @brief Destroys an MLIR context.
 * @param ctx The context to destroy.
 */
void alkyl_mlir_destroy_context(AlkylMlirContext ctx);

/**
 * @brief Dumps an MLIR module to a file.
 * @param mod The module.
 * @param filename The output filename.
 */
void alkyl_mlir_dump_module(AlkylMlirModule mod, const char* filename);

/**
 * @brief Adds a function to an MLIR module.
 * @param ctx The MLIR context.
 * @param mod The module.
 * @param name The function name.
 * @param is_extern Whether the function is external.
 * @param num_args Number of arguments.
 * @return The new function.
 */
AlkylMlirFunc alkyl_mlir_add_function(AlkylMlirContext ctx, AlkylMlirModule mod, const char* name, int is_extern, int num_args);

/**
 * @brief Gets an argument of an MLIR function.
 * @param func The function.
 * @param index The argument index.
 * @return The argument value.
 */
AlkylMlirValue alkyl_mlir_get_arg(AlkylMlirFunc func, int index);

/**
 * @brief Adds a basic block to an MLIR function.
 * @param func The function.
 * @return The new block.
 */
AlkylMlirBlock alkyl_mlir_add_block(AlkylMlirFunc func);

/**
 * @brief Sets the insertion point to the end of a block.
 * @param ctx The MLIR context.
 * @param block The block.
 */
void alkyl_mlir_set_insertion_point_to_end(AlkylMlirContext ctx, AlkylMlirBlock block);

/**
 * @brief Builds a return statement.
 * @param ctx The MLIR context.
 * @param val The return value.
 */
void alkyl_mlir_build_return(AlkylMlirContext ctx, AlkylMlirValue val);

/**
 * @brief Checks if the current block is terminated.
 * @param ctx The MLIR context.
 * @return Non-zero if terminated.
 */
int alkyl_mlir_is_terminated(AlkylMlirContext ctx);

/**
 * @brief Builds an alloca for an object.
 * @param ctx The MLIR context.
 * @param num_fields Number of fields.
 * @return The allocated pointer.
 */
AlkylMlirValue alkyl_mlir_build_alloc_object(AlkylMlirContext ctx, int num_fields);

/**
 * @brief Builds a store to a field.
 * @param ctx The MLIR context.
 * @param val The value to store.
 * @param ptr The pointer to the struct.
 * @param index The field index.
 */
void alkyl_mlir_build_store_field(AlkylMlirContext ctx, AlkylMlirValue val, AlkylMlirValue ptr, int index);

/**
 * @brief Builds a load from a field.
 * @param ctx The MLIR context.
 * @param ptr The pointer to the struct.
 * @param index The field index.
 * @param is_string Whether the field is a string.
 * @return The loaded value.
 */
AlkylMlirValue alkyl_mlir_build_load_field(AlkylMlirContext ctx, AlkylMlirValue ptr, int index, int is_string);

/**
 * @brief Checks if a block has a terminator.
 * @param block The block.
 * @return Non-zero if the block has a terminator.
 */
int alkyl_mlir_block_has_terminator(AlkylMlirBlock block);

/**
 * @brief Builds an alloca instruction.
 * @param ctx The MLIR context.
 * @param name The variable name.
 * @return The allocated value.
 */
AlkylMlirValue alkyl_mlir_build_alloca(AlkylMlirContext ctx, const char* name);

/**
 * @brief Builds a store instruction.
 * @param ctx The MLIR context.
 * @param val The value to store.
 * @param ptr The pointer to store to.
 */
void alkyl_mlir_build_store(AlkylMlirContext ctx, AlkylMlirValue val, AlkylMlirValue ptr);

/**
 * @brief Builds a load instruction.
 * @param ctx The MLIR context.
 * @param ptr The pointer to load from.
 * @return The loaded value.
 */
AlkylMlirValue alkyl_mlir_build_load(AlkylMlirContext ctx, AlkylMlirValue ptr);

/**
 * @brief Builds an integer constant.
 * @param ctx The MLIR context.
 * @param val The integer value.
 * @return The constant value.
 */
AlkylMlirValue alkyl_mlir_build_int_constant(AlkylMlirContext ctx, int val);

/**
 * @brief Builds a string constant.
 * @param ctx The MLIR context.
 * @param mod The MLIR module.
 * @param str The string value.
 * @return The constant value.
 */
AlkylMlirValue alkyl_mlir_build_string_constant(AlkylMlirContext ctx, AlkylMlirModule mod, const char* str);

/**
 * @brief Builds an add instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_add(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a subtract instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_sub(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a multiply instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_mul(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a divide instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_div(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a modulo instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_mod(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a shift-left instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_shl(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a shift-right instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_shr(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a bitwise-and instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_and(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a bitwise-or instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_or(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a bitwise-xor instruction.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_xor(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds an equality comparison.
 * @param ctx The MLIR context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_eq(AlkylMlirContext ctx, AlkylMlirValue lhs, AlkylMlirValue rhs);

/**
 * @brief Builds a panic instruction.
 * @param ctx The MLIR context.
 * @param err_id The error ID.
 * @param msg The error message.
 */
void alkyl_mlir_build_panic(AlkylMlirContext ctx, int err_id, const char* msg);

/**
 * @brief Builds a function call.
 * @param ctx The MLIR context.
 * @param name The function name.
 * @param args The argument array.
 * @param num_args Number of arguments.
 * @return The result value.
 */
AlkylMlirValue alkyl_mlir_build_call(AlkylMlirContext ctx, const char* name, AlkylMlirValue* args, int num_args);

/**
 * @brief Starts an if-else block.
 * @param ctx The MLIR context.
 * @param cond The condition value.
 * @param has_else Whether there is an else branch.
 * @return The if operation handle.
 */
void* alkyl_mlir_build_scf_if_start(AlkylMlirContext ctx, AlkylMlirValue cond, int has_else);

/**
 * @brief Begins the else branch of an if.
 * @param ctx The MLIR context.
 * @param if_op_ptr The if operation handle.
 */
void alkyl_mlir_build_scf_if_else(AlkylMlirContext ctx, void* if_op_ptr);

/**
 * @brief Ends an if-else block.
 * @param ctx The MLIR context.
 * @param if_op_ptr The if operation handle.
 */
void alkyl_mlir_build_scf_if_end(AlkylMlirContext ctx, void* if_op_ptr);

/**
 * @brief Starts a while loop.
 * @param ctx The MLIR context.
 * @return The while operation handle.
 */
void* alkyl_mlir_build_scf_while_start(AlkylMlirContext ctx);

/**
 * @brief Yields a condition for a while loop.
 * @param ctx The MLIR context.
 * @param while_op_ptr The while operation handle.
 * @param cond The condition value.
 */
void alkyl_mlir_build_scf_while_cond_yield(AlkylMlirContext ctx, void* while_op_ptr, AlkylMlirValue cond);

/**
 * @brief Ends a while loop.
 * @param ctx The MLIR context.
 * @param while_op_ptr The while operation handle.
 */
void alkyl_mlir_build_scf_while_end(AlkylMlirContext ctx, void* while_op_ptr);

/**
 * @brief Builds a break instruction.
 * @param ctx The MLIR context.
 */
void alkyl_mlir_build_scf_break(AlkylMlirContext ctx);

/**
 * @brief Builds a continue instruction.
 * @param ctx The MLIR context.
 */
void alkyl_mlir_build_scf_continue(AlkylMlirContext ctx);

/**
 * @brief Starts a switch block.
 * @param ctx The MLIR context.
 * @param cond The condition value.
 * @param num_cases Number of cases.
 * @return The switch operation handle.
 */
void* alkyl_mlir_build_switch_start(AlkylMlirContext ctx, AlkylMlirValue cond, int num_cases);

/**
 * @brief Starts a switch case.
 * @param ctx The MLIR context.
 * @param switch_op_ptr The switch operation handle.
 * @param val The case value.
 * @param is_leak Whether this is a leak case.
 * @return The case block.
 */
void* alkyl_mlir_build_switch_case_start(AlkylMlirContext ctx, void* switch_op_ptr, AlkylMlirValue val, int is_leak);

/**
 * @brief Ends a switch case.
 * @param ctx The MLIR context.
 * @param switch_op_ptr The switch operation handle.
 * @param is_leak Whether this is a leak case.
 */
void alkyl_mlir_build_switch_case_end(AlkylMlirContext ctx, void* switch_op_ptr, int is_leak);

/**
 * @brief Starts the default case of a switch.
 * @param ctx The MLIR context.
 * @param switch_op_ptr The switch operation handle.
 */
void alkyl_mlir_build_switch_default_start(AlkylMlirContext ctx, void* switch_op_ptr);

/**
 * @brief Ends the default case of a switch.
 * @param ctx The MLIR context.
 * @param switch_op_ptr The switch operation handle.
 */
void alkyl_mlir_build_switch_default_end(AlkylMlirContext ctx, void* switch_op_ptr);

/**
 * @brief Ends a switch block.
 * @param ctx The MLIR context.
 * @param switch_op_ptr The switch operation handle.
 */
void alkyl_mlir_build_switch_end(AlkylMlirContext ctx, void* switch_op_ptr);

#ifdef __cplusplus
}
#endif

#endif // ALKYL_MLIR_WRAPPER_H
