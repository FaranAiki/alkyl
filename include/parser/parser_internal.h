/**
 * @file parser_internal.h
 * @brief Internal parser declarations, macros, and data structures.
 */
#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "parser.h"
#include "../common/diagnostic.h"
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

// --- MODIFIER DEFINITIONS ---
#define MODIFIER_PUBLIC    (1 << 0)
#define MODIFIER_PRIVATE   (1 << 1)
#define MODIFIER_OPEN      (1 << 2)
#define MODIFIER_CLOSED    (1 << 3)
#define MODIFIER_CONST     (1 << 4)
#define MODIFIER_INERT     (1 << 6)
#define MODIFIER_REACTIVE  (1 << 7)
#define MODIFIER_NAKED     (1 << 8)
#define MODIFIER_STATIC    (1 << 9)
#define MODIFIER_PURE      (1 << 10)
#define MODIFIER_IMPURE    (1 << 11)
#define MODIFIER_PRISTINE  (1 << 12)
#define MODIFIER_TAINTED   (1 << 13)
#define MODIFIER_COVALENT  (1 << 14)
#define MODIFIER_ABSTRACT  (1 << 15)
#define MODIFIER_EXACT     (1 << 16)
#define MODIFIER_PRAGMA    (1 << 17)
#define MODIFIER_METHOD    (1 << 18)
#define MODIFIER_CONTAINER (1 << 19)
#define MODIFIER_FRAME     (1 << 20)
#define MODIFIER_META      (1 << 21)
#define MODIFIER_TOTAL     (1 << 22)
#define MODIFIER_PARTIAL   (1 << 23)
#define MODIFIER_EXTENDED  (1 << 24)
#define MODIFIER_OVERRIDE  (1 << 25)
#define MODIFIER_MUTABLE   (1 << 26)
#define MODIFIER_EXPORT    (1 << 27)

/**
 * @brief Parses a sequence of modifier tokens and returns a bitmask.
 * @param p Parser context.
 * @return Bitmask of parsed modifier flags (MODIFIER_* constants).
 */
int parse_modifiers(Parser* p);

/**
 * @brief Applies a modifier bitmask to a class node.
 * @param node Class node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
void apply_class_modifiers(ClassNode* node, int modifiers);

/**
 * @brief Applies a modifier bitmask to a function definition node.
 * @param node Function definition node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
void apply_func_modifiers(FuncDefNode* node, int modifiers);

/**
 * @brief Parses an initializer expression or constructor-call style initializer.
 * @param p Parser context.
 * @param vtype Expected type for the initializer (used for constructor-call style).
 * @return AST node for the initializer, or NULL if not present.
 */
ASTNode* parse_initializer(Parser *p, VarType vtype);

/**
 * @brief Applies a modifier bitmask to a variable declaration node.
 * @param node Variable declaration node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
void apply_var_modifiers(VarDeclNode* node, int modifiers);

/**
 * @brief Dispatches modifier application to the appropriate typed function.
 * @param node Generic AST node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
void apply_modifiers_to_node(ASTNode *node, int modifiers);

/**
 * @brief Applies a modifier bitmask to a parameter node.
 * @param param Parameter node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
void apply_param_modifiers(Parameter* param, int modifiers);

struct Macro {
    char *name;
    char **params;
    int param_count;
    Token *body;
    int body_len;
    struct Macro *next;
};

struct TypeName {
    char *name;
    int is_enum; 
    struct TypeName *next;
};

struct TypeAlias {
    char *name;
    VarType target;
    struct TypeAlias *next;
};

struct Expansion {
    Token *tokens;
    int count;
    int pos;
    char *macro_name; /* macro that produced this expansion (blue-paint) */
    struct Expansion *next;
};

/**
 * @brief Reports a parse error and marks the parser as having an error.
 * @param p Parser context.
 * @param msg Error message.
 */
void parser_fail(Parser *p, const char *msg);

/**
 * @brief Reports a parse error at a specific token location.
 * @param p Parser context.
 * @param t Token at which the error occurred.
 * @param msg Error message.
 */
void parser_fail_at(Parser *p, Token t, const char *msg);

/**
 * @brief Synchronizes the parser after an error by consuming tokens until a safe resync point.
 * @param p Parser context.
 */
void parser_sync(Parser *p);

/**
 * @brief Consumes the current token if it matches the expected type, otherwise reports an error.
 * @param p Parser context.
 * @param type Expected token type.
 */
void eat(Parser *p, TokenType type);

/**
 * @brief Parses a type specification from the current token stream.
 * @param p Parser context.
 * @return Parsed VarType.
 */
VarType parse_type(Parser *p);

/**
 * @brief Parses a function pointer declaration.
 * @param p Parser context.
 * @param ret_type Return type of the function pointer.
 * @param out_name Output parameter receiving the function pointer name (may be NULL).
 * @return Parsed VarType for the function pointer.
 */
VarType parse_func_ptr_decl(Parser *p, VarType ret_type, char **out_name);

/**
 * @brief Allocates memory for a parser AST node, zeroing it and attaching source info.
 * @param p Parser context.
 * @param size Number of bytes to allocate.
 * @return Pointer to zeroed memory with source info attached.
 */
void* parser_alloc(Parser *p, size_t size);

/**
 * @brief Allocates raw memory for parser use, zeroing it.
 * @param p Parser context.
 * @param size Number of bytes to allocate.
 * @return Pointer to zeroed memory.
 */
void* parser_alloc_raw(Parser *p, size_t size);

/**
 * @brief Duplicates a string using the parser's arena allocator.
 * @param p Parser context.
 * @param str String to duplicate.
 * @return Duplicated string, or NULL if str is NULL.
 */
char* parser_strdup(Parser *p, const char *str);

/**
 * @brief Registers a macro in the parser's macro table.
 * @param p Parser context.
 * @param name Macro name.
 * @param params Parameter names (NULL for object-like macros).
 * @param param_count Number of parameters.
 * @param body Replacement tokens for the macro body.
 * @param body_len Length of the body token array.
 */
void register_macro(Parser *p, const char *name, char **params, int param_count, Token *body, int body_len);

/**
 * @brief Registers a type name in the parser's type map.
 * @param p Parser context.
 * @param name Name of the type.
 * @param is_enum Non-zero if the type is an enum.
 */
void register_typename(Parser *p, const char *name, int is_enum);

/**
 * @brief Checks whether a name is registered as a type.
 * @param p Parser context.
 * @param name Type name to check.
 * @return Non-zero if the name is a known type.
 */
int is_typename(Parser *p, const char *name);

/**
 * @brief Checks whether the current token starts a type declaration.
 * @param p Parser context.
 * @return Non-zero if the current token begins a type.
 */
int is_type_start(Parser *p);

/**
 * @brief Registers or updates a type alias.
 * @param p Parser context.
 * @param name Alias name.
 * @param target Target VarType for the alias.
 */
void register_alias(Parser *p, const char *name, VarType target);

/**
 * @brief Looks up a registered type alias.
 * @param p Parser context.
 * @param name Alias name to look up.
 * @return Pointer to the alias VarType, or NULL if not found.
 */
VarType* get_alias(Parser *p, const char *name);

/**
 * @brief Expands macros starting from a given token.
 * @param p Parser context.
 * @param t Starting token for expansion.
 * @return The first token after expansion.
 */
Token expand_macros_from(Parser *p, Token t);

/**
 * @brief Returns the next raw token from the parser's token buffer.
 * @param p Parser context.
 * @return Next token, or TOKEN_EOF if exhausted.
 */
Token lexer_next_raw(Parser *p);

// Expressions (parser/expr.c)

/**
 * @brief Parses a function/method call expression.
 * @param p Parser context.
 * @param target Target expression (e.g., member access for method calls).
 * @return AST node for the call, or NULL on error.
 */
ASTNode* parse_call(Parser *p, ASTNode *target);

/**
 * @brief Parses postfix operators (member access, array index, call) on an expression.
 * @param p Parser context.
 * @param node Base expression node.
 * @return AST node after applying postfix operators.
 */
ASTNode* parse_postfix(Parser *p, ASTNode *node); 

/**
 * @brief Parses an expression using precedence climbing.
 * @param p Parser context.
 * @return Parsed expression AST node, or NULL on error.
 */
ASTNode* parse_expression(Parser *p);

#include "modif.h"
#include "stmt.h"
#include "top.h"
#include "semantic.h"


/**
 * @brief Consumes a semicolon if present; accepts implicit termination otherwise.
 * @param p Parser context.
 */
void eat_semi(Parser *p);

/**
 * @brief Sets the source location (line and column) on an AST node.
 * @param n AST node to update (may be NULL).
 * @param line Source line number.
 * @param col Source column number.
 */
void set_loc(ASTNode *n, int line, int col);

#include "parser/fragment/class.h"
#include "parser/fragment/cond.h"
#include "parser/fragment/loop.h"
#include "parser/fragment/decl.h"

#endif // PARSER_INTERNAL_H
