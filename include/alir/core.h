/**
 * @file core.h
 * @brief Core ALIR module and function creation.
 */
#ifndef ALIR_CORE_H
#define ALIR_CORE_H

/**
 * @brief Creates a new ALIR module.
 * @param ctx The compiler context.
 * @param name The module name.
 * @return The new ALIR module.
 */
AlirModule* alir_create_module(CompilerContext *ctx, const char *name);

/**
 * @brief Adds a function to an ALIR module.
 * @param mod The ALIR module.
 * @param name The function name.
 * @param ret The return type.
 * @param is_flux Whether the function is a flux (generator).
 * @return The new ALIR function.
 */
AlirFunction* alir_add_function(AlirModule *mod, const char *name, VarType ret, int is_flux);

/**
 * @brief Adds a parameter to an ALIR function.
 * @param mod The ALIR module.
 * @param func The function to add the parameter to.
 * @param name The parameter name.
 * @param type The parameter type.
 */
void alir_func_add_param(AlirModule *mod, AlirFunction *func, const char *name, VarType type);

/**
 * @brief Adds a string literal to the module's global constants.
 * @param mod The ALIR module.
 * @param content The string content.
 * @param type The string type.
 * @return The created ALIR value.
 */
AlirValue* alir_module_add_string_literal(AlirModule *mod, const char *content, VarType type);

/**
 * @brief Adds a new basic block to a function.
 * @param mod The ALIR module.
 * @param func The function to add the block to.
 * @param label_hint A hint for the block label.
 * @return The new ALIR block.
 */
AlirBlock* alir_add_block(AlirModule *mod, AlirFunction *func, const char *label_hint);

/**
 * @brief Appends an instruction to a basic block.
 * @param block The block to append to.
 * @param inst The instruction to append.
 */
void alir_append_inst(AlirBlock *block, AlirInst *inst);

#endif // ALIR_CORE_H
