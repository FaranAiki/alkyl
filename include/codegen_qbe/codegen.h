/**
 * @file codegen.h
 * @brief QBE code generation interface for ALIR.
 */
#ifndef QBE_CODEGEN_H
#define QBE_CODEGEN_H

#include "codegen/codegen.h"

/**
 * @brief Returns the QBE type character for an Alkyl type.
 * @param t The Alkyl type.
 * @return The QBE type character.
 */
char qbe_type(VarType t);

/**
 * @brief Returns the size of a QBE type.
 * @param qtype The QBE type character.
 * @return The size in bytes.
 */
int qbe_type_size(char qtype);

/**
 * @brief Prints an ALIR value.
 * @param out The output stream.
 * @param v The ALIR value.
 */
void print_val(FILE *out, AlirValue *v);

/**
 * @brief Emits a single ALIR instruction.
 * @param out The output stream.
 * @param module The ALIR module.
 * @param inst The instruction.
 * @param next_block The next block (for branch targets).
 */
void emit_inst(FILE *out, AlirModule *module, AlirInst *inst, AlirBlock *next_block);

/**
 * @brief Finds the maximum temporary ID in a module.
 * @param module The ALIR module.
 * @return The maximum temporary ID.
 */
int find_max_temp(AlirModule *module);

/**
 * @brief Allocates a new QBE temporary.
 * @return The new temporary ID.
 */
int alloc_qbe_temp(void);

extern int s_next_qbe_temp;
extern AlirFunction *s_current_qbe_function;

#endif // QBE_CODEGEN_H
