/**
 * @file emitter.h
 * @brief ALIR emission and printing utilities.
 */
#ifndef ALIR_EMITTER_H
#define ALIR_EMITTER_H
#include "alir.h"

/**
 * @brief Emits the ALIR module to a file stream.
 * @param mod The ALIR module.
 * @param f The output file stream.
 */
void alir_emit_stream(AlirModule *mod, FILE *f);

/**
 * @brief Prints the ALIR module to stderr.
 * @param mod The ALIR module.
 */
void alir_print(AlirModule *mod);

/**
 * @brief Emits the ALIR module to a file.
 * @param mod The ALIR module.
 * @param filename The output file path.
 */
void alir_emit_to_file(AlirModule *mod, const char *filename);

/**
 * @brief Generates ALIR from a semantic context and AST.
 * @param sem The semantic context.
 * @param root The root AST node.
 * @return The generated ALIR module.
 */
AlirModule* alir_generate(SemanticCtx *sem, ASTNode *root);

/**
 * @brief Prints a type to a file stream.
 * @param f The output file stream.
 * @param t The type to print.
 */
void alir_fprint_type(FILE *f, VarType t);

/**
 * @brief Prints an ALIR value to a file stream.
 * @param f The output file stream.
 * @param v The value to print.
 */
void alir_fprint_val(FILE *f, AlirValue *v);

#endif // ALIR_EMITTER_H
