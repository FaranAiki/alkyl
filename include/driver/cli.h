#ifndef CLI_H
#define CLI_H

#include "../metalir/metalir.h"

int run_repl(void);
int run_file(const char *filename);
int import_module(const char *module_name);

#endif
