#ifndef PARSER_EMITTER_H
#define PARSER_EMITTER_H

#include "parser.h"
#include "../common/common.h"

// Traverses the AST and reconstructs the source code into a string
char* parser_to_string(ASTNode *root);

// Traverses the AST and writes the reconstructed source code to a file
void parser_to_file(ASTNode *root, const char *filename);

// Helper: Parses the source string into AST, converts AST to string, frees AST
char* parser_string_to_string(const char *src);

// Helper: Parses the source string into AST, writes to file, frees AST
void parser_string_to_file(const char *src, const char *filename);

#endif // PARSER_EMITTER_H
