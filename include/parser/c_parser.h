/**
 * @file c_parser.h
 * @brief C header file parser for the Alkyl compiler.
 */
#ifndef C_PARSER_H
#define C_PARSER_H

#include <stdbool.h>
#include "typestruct.h"
#include "../common/context.h"
#include "../common/hashmap.h"
#include "c_lexer.h"

typedef struct CParser CParser;

struct CParser {
    CLexer lexer;
    CToken current;
    CompilerContext *ctx;
    int has_error;

    // Typedef resolution table
    HashMap typedef_map;
    struct {
        char **names;
        VarType *types;
        int count;
        int capacity;
    } typedefs;

    // Macro definitions from #define
    struct {
        char **names;
        char **values;
        int count;
        int capacity;
    } defines;

    // Conditional compilation stack
    struct {
        int *active;
        int count;
        int capacity;
    } cond_stack;
};

/**
 * @brief Initialize a C parser for a given source file.
 * @param p C parser to initialize.
 * @param ctx Compiler context.
 * @param filename Source file name (for error reporting).
 * @param source Source code string.
 */
void c_parser_init(CParser *p, CompilerContext *ctx, const char *filename, const char *source);

/**
 * @brief Preprocess a C header file using the system C preprocessor.
 * @param ctx Compiler context.
 * @param fname Header file path to preprocess.
 * @return Preprocessed source code string, or NULL on error.
 */
char* c_preprocess_header(CompilerContext *ctx, const char *fname);

/**
 * @brief Parse a preprocessed C header into an AST.
 * @param p Initialized C parser.
 * @return Root AST node (list of declarations), or NULL on error.
 */
ASTNode* c_parse_header(CParser *p);

#endif
