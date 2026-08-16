/**
 * @file metalir.h
 * @brief Metalir runner interface for executing Alkyl code.
 */
#ifndef METALIR_H
#define METALIR_H

#include "parser/parser.h"
#include "semantic/semantic.h"
#include "alir/alir.h"
#include "common/arena.h"
#include "common/context.h"
#include "metalir/vm.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MetalirRunner {
    Arena ast_arena;
    CompilerContext ctx;
    Lexer lexer;
    Parser parser;
    SemanticCtx sem;
    AlirModule *module;
    Arena vm_arena;
    MetalirVM *vm;
} MetalirRunner;

/**
 * @brief Creates a new Metalir runner.
 * @param module_name The name of the module.
 * @param sem_settings Semantic analysis settings.
 * @param function_call_require_comma Whether commas are required in function calls.
 * @return The new runner.
 */
MetalirRunner* metalir_runner_create(const char *module_name,
                                      const SemanticSettings *sem_settings,
                                      int function_call_require_comma);

/**
 * @brief Destroys a Metalir runner.
 * @param r The runner to destroy.
 */
void metalir_runner_destroy(MetalirRunner *r);

/**
 * @brief Parses source code.
 * @param r The Metalir runner.
 * @param source The source string.
 * @param filename The source filename.
 * @param settings Lexer settings.
 * @return The root AST node.
 */
ASTNode* metalir_parse(MetalirRunner *r, const char *source,
                       const char *filename, const LexerSettings *settings);

/**
 * @brief Runs semantic checks on an AST.
 * @param r The Metalir runner.
 * @param root The root AST node.
 */
void metalir_sem_check(MetalirRunner *r, ASTNode *root);

/**
 * @brief Generates ALIR from an AST.
 * @param r The Metalir runner.
 * @param root The root AST node.
 */
void metalir_alir_generate(MetalirRunner *r, ASTNode *root);

/**
 * @brief Executes an ALIR function.
 * @param r The Metalir runner.
 * @param func_name The function name.
 * @return The return value.
 */
long long metalir_execute_alir(MetalirRunner *r, const char *func_name);

/**
 * @brief Parses and executes an AST directly.
 * @param r The Metalir runner.
 * @param root The root AST node.
 * @param source The source string.
 * @param filename The source filename.
 * @return The return value.
 */
long long metalir_execute_parse(MetalirRunner *r, ASTNode *root,
                                 const char *source, const char *filename);

/**
 * @brief Parses and executes a source string.
 * @param r The Metalir runner.
 * @param source The source string.
 * @param filename The source filename.
 * @return The return value.
 */
long long metalir_execute_string(MetalirRunner *r, const char *source,
                                  const char *filename);

/**
 * @brief Loads a compiled module.
 * @param r The Metalir runner.
 * @param path The path to the module file.
 * @return 0 on success, non-zero on failure.
 */
int metalir_load_module(MetalirRunner *r, const char *path);

/**
 * @brief Resolves imports in an AST.
 * @param r The Metalir runner.
 * @param root Pointer to the root AST node.
 */
void metalir_resolve_imports(MetalirRunner *r, ASTNode **root);

/**
 * @brief Runs a variable declaration.
 * @param r The Metalir runner.
 * @param vd The variable declaration node.
 * @param seq The sequence number.
 * @return The evaluated value.
 */
long long metalir_run_var_decl(MetalirRunner *r, VarDeclNode *vd, int seq);

/**
 * @brief Runs a class definition.
 * @param r The Metalir runner.
 * @param curr The current AST node.
 * @param root The root AST node.
 */
void metalir_run_class(MetalirRunner *r, ASTNode *curr, ASTNode *root);

/**
 * @brief Runs a function definition.
 * @param r The Metalir runner.
 * @param curr The function definition AST node.
 */
void metalir_run_func_def(MetalirRunner *r, ASTNode *curr);

/**
 * @brief Runs a link statement.
 * @param r The Metalir runner.
 * @param ln The link node.
 */
void metalir_run_link(MetalirRunner *r, LinkNode *ln);

/**
 * @brief Runs an expression.
 * @param r The Metalir runner.
 * @param curr The expression AST node.
 * @param seq The sequence number.
 * @param print_rich Whether to print rich output.
 * @param out_type Output parameter for the result type.
 * @return The evaluated value.
 */
long long metalir_run_expr(MetalirRunner *r, ASTNode *curr, int seq,
                           int print_rich, VarType *out_type);

/**
 * @brief Prints a REPL value.
 * @param rt The result type.
 * @param result The result value.
 */
void metalir_print_repl_value(VarType rt, long long result);

#ifdef __cplusplus
}
#endif

#endif
