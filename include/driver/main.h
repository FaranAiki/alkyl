/**
 * @file main.h
 * @brief Main driver declarations.
 */
#ifndef MAIN_H
#define MAIN_H

#include <stdbool.h>
#include "../../include/codegen/codegen.h"
#include "../../include/parser/parser.h"
#include "../../include/semantic/semantic.h"
#include "../../include/parser/parser.h"
#include "../../include/semantic/semantic.h"
#include "../../include/common/debug.h"
#include "../../include/alir/alir.h"
#include "../include/alick/alick.h"

/**
 * @brief Reads a file into a malloc'd buffer.
 * @param filename The path to the file.
 * @return A null-terminated buffer, or NULL on failure.
 */
char* read_file(const char* filename);

#endif
