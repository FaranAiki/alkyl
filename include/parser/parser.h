/**
 * @file parser.h
 * @brief Main parser interface for the Alkyl compiler.
 */
#ifndef PARSER_H
#define PARSER_H

#include "../lexer/lexer.h"
#include "../common/debug.h"
#include "../common/context.h"
#include <stdbool.h>
#include "../common/hashmap.h"

#include "parser/typestruct.h"
#include "semantic/typestruct.h"

typedef struct Macro Macro;
typedef struct TypeName TypeName;
typedef struct TypeAlias TypeAlias;
typedef struct Expansion Expansion;

/**
 * @brief Parser configuration settings.
 */
typedef struct {
    int require_parens_for_conditions;
    int allow_implicit_return;
    int allow_postfix_types;       // e.g., let x: int
    int strict_boolean_conditions; // e.g., if (1) vs if (true)
    int allow_vector_initialization;
    int namespace_auto_search;     // default 1
    int namespace_ausearch_warning;// default 1
    int greedy_space_calls;        // default 0
    int function_call_require_comma; // default 1
    int array_separator_with_space; // default 0
    int multiplication_if_digit_word; // default 0: 2pi -> 2*pi
    int exponentation_if_word_digit;  // default 0: pi2 -> pi**2 (future)
    int function_auto_call;          // default 0: allow calling member access without parens
    const char **import_paths;
    int import_path_count;
} ParserSettings;

/**
 * @brief The main parser state structure.
 */
typedef struct Parser {
    Lexer *l;
    CompilerContext *ctx;
    ParserSettings settings;
    
    Token current_token;
    int has_error;
    
    Macro *macro_head;
    TypeName *type_head;
    TypeAlias *alias_head;
    struct Expansion *expansion_head;
    int disable_macro_expansion;
    Token *tokens;
    int token_count;
    int token_capacity;
    int token_pos;
    HashMap types_map;
    char *current_namespace;
    char *pending_cconv;
    struct ASTNode *synthetic_classes;
    int in_space_separated_call;
    int disable_space_call;
} Parser;

/**
 * @brief Initializes the parser with a lexer and settings.
 * @param p The parser to initialize.
 * @param l The lexer to read tokens from.
 * @param settings Parser configuration settings, or NULL for defaults.
 */
void parser_init(Parser *p, Lexer *l, ParserSettings *settings);

/**
 * @brief Returns the next token, expanding macros as needed.
 * @param p The parser.
 * @return The next expanded token.
 */
Token get_next_token_expanded(Parser *p);

/**
 * @brief Reports an error through the lexer's diagnostic system.
 * @param l The lexer instance.
 * @param t The token associated with the error.
 * @param msg The error message.
 */
void report_error(Lexer *l, Token t, const char *msg);

/**
 * @brief Deep-clones an AST node, optionally substituting types and renaming identifiers.
 * @param ctx The compiler context.
 * @param node The AST node to clone.
 * @param type_params Type parameter names to replace.
 * @param replace_with Replacement types for type parameters.
 * @param num_params Number of type parameters.
 * @param rename_from Original identifiers to rename.
 * @param rename_to New identifier names.
 * @param num_renames Number of renames.
 * @return A cloned AST node, or NULL on failure.
 */
ASTNode* ast_clone(CompilerContext *ctx, ASTNode *node, char **type_params, VarType *replace_with, int num_params, char **rename_from, char **rename_to, int num_renames);

/**
 * @brief Rewrites a macro invocation AST by substituting macro arguments.
 * @param ctx The compiler context.
 * @param node The AST node to rewrite.
 * @param varargs_head Head of the varargs list, or NULL.
 * @param param_names Macro parameter names.
 * @param param_args Macro argument AST nodes.
 * @param num_params Number of parameters.
 * @return The rewritten AST node.
 */
ASTNode* ast_rewrite_macro(CompilerContext *ctx, ASTNode *node, ASTNode *varargs_head, char **param_names, ASTNode **param_args, int num_params);

/**
 * @brief Clones a VarType, optionally substituting type parameters and renaming.
 * @param ctx The compiler context.
 * @param t The type to clone.
 * @param type_params Type parameter names to replace.
 * @param replace_with Replacement types.
 * @param num_params Number of type parameters.
 * @param rename_from Original identifiers to rename.
 * @param rename_to New identifier names.
 * @param num_renames Number of renames.
 * @return The cloned VarType.
 */
VarType clone_var_type(CompilerContext *ctx, VarType t, char **type_params, VarType *replace_with, int num_params, char **rename_from, char **rename_to, int num_renames);

/**
 * @brief Peeks at the next token without consuming it.
 * @param p The parser.
 * @return The next token.
 */
Token parser_peek_token(Parser *p);

/**
 * @brief Peeks at a token n positions ahead.
 * @param p The parser.
 * @param offset Number of tokens to peek ahead.
 * @return The token at the given offset.
 */
Token parser_peek_token_n(Parser *p, int offset);

/**
 * @brief Pre-scans tokens to build lookahead buffers.
 * @param p The parser.
 */
void parser_prescan(Parser *p);

/**
 * @brief Sets default import paths in the parser settings.
 * @param ps The parser settings to populate.
 */
void parser_set_default_import_paths(ParserSettings *ps);

/**
 * @brief Parses the entire source program.
 * @param p The parser.
 * @return The root AST node.
 */
ASTNode* parse_program(Parser *p);

/**
 * @brief Parses a single expression.
 * @param p The parser.
 * @return The parsed expression AST node.
 */
ASTNode* parse_expression(Parser *p);

#include "emitter.h"
#include "link.h"
#include "modifier.h"

#endif
