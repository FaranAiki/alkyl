/**
 * @file core.h
 * @brief Core parser infrastructure: import resolution, token peeking, and program parsing.
 */
#ifndef PARSER_CORE_H
#define PARSER_CORE_H

#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads and returns the content of an imported file, searching multiple extensions and paths.
 * @param p Parser context.
 * @param filename Name of the file to read (without extension).
 * @return File content as a null-terminated string, or NULL on failure.
 */
char* read_import_file(Parser *p, const char *filename);

#ifdef __cplusplus
}
#endif

#endif // PARSER_CORE_H
