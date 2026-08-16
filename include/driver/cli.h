/**
 * @file cli.h
 * @brief Command-line interface declarations.
 */
#ifndef CLI_H
#define CLI_H

#include "../metalir/metalir.h"

/**
 * @brief Runs the REPL.
 * @return 0 on success, non-zero on failure.
 */
int run_repl(void);

/**
 * @brief Runs a source file.
 * @param filename The path to the source file.
 * @return 0 on success, non-zero on failure.
 */
int run_file(const char *filename);

/**
 * @brief Imports a module.
 * @param module_name The module name.
 * @return 0 on success, non-zero on failure.
 */
int import_module(const char *module_name);

#endif
