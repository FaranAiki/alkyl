/**
 * @file semantic.h
 * @brief Semantic analysis interface for the Alkyl compiler.
 */
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../parser/parser.h"
#include "../common/common.h"
#include "../common/diagnostic.h"
#include "../common/context.h"
#include <stdbool.h>

#include "typestruct.h"

/**
 * @brief Initializes the semantic analysis context.
 * @param ctx The semantic context to initialize.
 * @param compiler_ctx The compiler context.
 * @param settings Semantic analysis settings.
 */
void sem_init(SemanticCtx *ctx, CompilerContext *compiler_ctx, SemanticSettings *settings);

/**
 * @brief Cleans up the semantic analysis context.
 * @param ctx The semantic context.
 */
void sem_cleanup(SemanticCtx *ctx);

/**
 * @brief Runs semantic checks on the entire program.
 * @param ctx The semantic context.
 * @param root The root AST node.
 * @return 0 on success, non-zero on failure.
 */
int sem_check_program(SemanticCtx *ctx, ASTNode *root);

/**
 * @brief Enters a new scope.
 * @param ctx The semantic context.
 * @param is_func Whether the scope is a function scope.
 * @param ret_type The return type of the function, if applicable.
 */
void sem_scope_enter(SemanticCtx *ctx, int is_func, VarType ret_type);

/**
 * @brief Exits the current scope.
 * @param ctx The semantic context.
 */
void sem_scope_exit(SemanticCtx *ctx);

/**
 * @brief Adds a symbol to the current scope.
 * @param ctx The semantic context.
 * @param name The symbol name.
 * @param kind The symbol kind.
 * @param type The symbol type.
 * @return The created symbol, or NULL on failure.
 */
SemSymbol* sem_symbol_add(SemanticCtx *ctx, const char *name, SymbolKind kind, VarType type);

/**
 * @brief Looks up a type symbol.
 * @param ctx The semantic context.
 * @param name The type name.
 * @return The symbol, or NULL if not found.
 */
SemSymbol* sem_symbol_lookup_type(SemanticCtx *ctx, const char *name);

/**
 * @brief Looks up a symbol in the current scopes.
 * @param ctx The semantic context.
 * @param name The symbol name.
 * @param out_scope Output parameter for the scope where the symbol was found.
 * @return The symbol, or NULL if not found.
 */
SemSymbol* sem_symbol_lookup(SemanticCtx *ctx, const char *name, SemScope **out_scope);

/**
 * @brief Looks up a symbol directly in a scope.
 * @param scope The scope to search.
 * @param name The symbol name.
 * @return The symbol, or NULL if not found.
 */
SemSymbol* find_in_scope_direct(SemScope *scope, const char *name);

/**
 * @brief Resolves overloaded functions.
 * @param ctx The semantic context.
 * @param args The argument AST nodes.
 * @param out_arg_count Output parameter for the argument count.
 * @param first_sym The first candidate symbol.
 * @param err_node The AST node to associate with errors.
 * @return The resolved symbol, or NULL on failure.
 */
SemSymbol* sem_resolve_overload(SemanticCtx *ctx, ASTNode **args, int *out_arg_count, SemSymbol *first_sym, ASTNode *err_node);

/**
 * @brief Sets the inferred type of an AST node.
 * @param ctx The semantic context.
 * @param node The AST node.
 * @param type The inferred type.
 */
void sem_set_node_type(SemanticCtx *ctx, ASTNode *node, VarType type);

/**
 * @brief Gets the inferred type of an AST node.
 * @param ctx The semantic context.
 * @param node The AST node.
 * @return The inferred type.
 */
VarType sem_get_node_type(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Marks an AST node as tainted.
 * @param ctx The semantic context.
 * @param node The AST node.
 * @param is_tainted Whether the node is tainted.
 */
void sem_set_node_tainted(SemanticCtx *ctx, ASTNode *node, int is_tainted);

/**
 * @brief Checks that a residue case is exhaustive.
 * @param ctx The semantic context.
 * @param where The AST node where the check occurs.
 * @param err_sym The error symbol.
 * @param cases The residue cases.
 * @param default_case Whether there is a default case.
 */
void sem_check_residue_exhaustive(SemanticCtx *ctx, ASTNode *where, SemSymbol *err_sym, ResidueCase *cases, int default_case);

/**
 * @brief Emits a fallback hint for residue handling.
 * @param ctx The semantic context.
 * @param condition The AST node for the condition.
 */
void sem_emit_fallback_hint(SemanticCtx *ctx, ASTNode *condition);

/**
 * @brief Marks an AST node as impure.
 * @param ctx The semantic context.
 * @param node The AST node.
 * @param is_impure Whether the node is impure.
 */
void sem_set_node_impure(SemanticCtx *ctx, ASTNode *node, int is_impure);

/**
 * @brief Gets whether an AST node is tainted.
 * @param ctx The semantic context.
 * @param node The AST node.
 * @return Non-zero if the node is tainted.
 */
int sem_get_node_tainted(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Gets whether an AST node is impure.
 * @param ctx The semantic context.
 * @param node The AST node.
 * @return Non-zero if the node is impure.
 */
int sem_get_node_impure(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Checks whether two types are compatible.
 * @param ctx The semantic context.
 * @param dest The destination type.
 * @param src The source type.
 * @return true if the types are compatible.
 */
bool sem_types_are_compatible(SemanticCtx *ctx, VarType dest, VarType src);

/**
 * @brief Checks whether a value can be cast to a type.
 * @param ctx The semantic context.
 * @param dest The destination type.
 * @param src The source type.
 * @return true if the cast is valid.
 */
bool sem_types_are_castable(SemanticCtx *ctx, VarType dest, VarType src);

/**
 * @brief Checks whether two types are equal.
 * @param a First type.
 * @param b Second type.
 * @return true if the types are equal.
 */
int sem_types_are_equal(VarType a, VarType b);

/**
 * @brief Converts a type to its string representation.
 * @param t The type.
 * @return A string describing the type.
 */
char* sem_type_to_str(VarType t);

/**
 * @brief Mangles a function name for the Alkyl ABI.
 * @param ctx The semantic context.
 * @param class_name The class name, or NULL.
 * @param base_name The base function name.
 * @param params The function parameters.
 * @return The mangled name.
 */
char* sem_mangle_func_name(SemanticCtx *ctx, const char *class_name, const char *base_name, Parameter *params);

/**
 * @brief Mangles a function name using the Itanium C++ ABI.
 * @param ctx The semantic context.
 * @param class_name The class name, or NULL.
 * @param base_name The base function name.
 * @param params The function parameters.
 * @return The mangled name.
 */
char* sem_mangle_itanium_func_name(SemanticCtx *ctx, const char *class_name, const char *base_name, Parameter *params);

/**
 * @brief Reports a semantic error.
 * @param ctx The semantic context.
 * @param node The AST node associated with the error.
 * @param fmt The printf-style format string.
 * @param ... Format arguments.
 */
void sem_error(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...);

/**
 * @brief Reports a semantic warning.
 * @param ctx The semantic context.
 * @param node The AST node associated with the warning.
 * @param fmt The printf-style format string.
 * @param ... Format arguments.
 */
void sem_warning(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...);

/**
 * @brief Reports semantic information.
 * @param ctx The semantic context.
 * @param node The AST node associated with the info.
 * @param fmt The printf-style format string.
 * @param ... Format arguments.
 */
void sem_info(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...);

/**
 * @brief Reports a semantic hint.
 * @param ctx The semantic context.
 * @param node The AST node associated with the hint.
 * @param fmt The printf-style format string.
 * @param ... Format arguments.
 */
void sem_hint(SemanticCtx *ctx, ASTNode *node, const char *fmt, ...);

/**
 * @brief Registers built-in symbols in the semantic context.
 * @param ctx The semantic context.
 */
void sem_register_builtins(SemanticCtx *ctx);

/**
 * @brief Runs semantic checks on an expression.
 * @param ctx The semantic context.
 * @param node The expression AST node.
 */
void sem_check_expr(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Scans top-level declarations.
 * @param ctx The semantic context.
 * @param node The AST node.
 */
void sem_scan_top_level(SemanticCtx *ctx, ASTNode *node);

/**
 * @brief Looks up a local symbol.
 * @param ctx The semantic context.
 * @param name The symbol name.
 * @return The symbol, or NULL if not found.
 */
SemSymbol* lookup_local_symbol(SemanticCtx *ctx, const char *name);

/**
 * @brief Runs semantic checks on a function definition.
 * @param ctx The semantic context.
 * @param node The function definition node.
 */
void sem_check_func_def(SemanticCtx *ctx, FuncDefNode *node);

/**
 * @brief Runs semantic checks on a variable declaration.
 * @param ctx The semantic context.
 * @param node The variable declaration node.
 * @param register_sym Whether to register the symbol in the symbol table.
 */
void sem_check_var_decl(SemanticCtx *ctx, VarDeclNode *node, int register_sym);

/**
 * @brief Inserts an implicit cast.
 * @param ctx The semantic context.
 * @param node_ptr Pointer to the AST node pointer.
 * @param target_type The target type.
 */
void sem_insert_implicit_cast(SemanticCtx *ctx, ASTNode **node_ptr, VarType target_type);

#include "emitter.h"
#include "type.h"
#include "check.h"
#include "fragment/lookup.h"
#include "fragment/switch.h"
#include "fragment/symbolic.h"
#include "fragment/field.h"
#include "fragment/block.h"
#include "modifier/class.h"
#include "modifier/func.h"
#include "modifier/taint.h"

#endif // SEMANTIC_H
