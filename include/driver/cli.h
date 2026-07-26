#ifndef CLI_H
#define CLI_H

#define INPUT_BUFFER_SIZE 4096

#include "../include/parser/parser.h"
#include "../include/common/debug.h"

// Entry points for Ethyl interpreter
int run_repl(void);
int run_file(const char *filename);
int import_module(const char *module_name);

#endif
