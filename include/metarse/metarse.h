/**
 * @file metarse.h
 * @brief Metarse executor interface for running Alkyl code.
 */
#ifndef METARSE_H
#define METARSE_H

#include "../common/arena.h"
#include "../common/context.h"
#include "../parser/parser.h"
#include "../semantic/semantic.h"
#include "../alir/alir.h"
#include <metalir/vm.h>

/**
 * @brief The Metarse executor context.
 */
typedef struct {
    Arena ast_arena;
    CompilerContext ctx;
    Parser p;
    SemanticCtx sem;
    AlirModule *module;
    Arena vm_arena;
    MetalirVM *vm;
} Executor;

/**
 * @brief Initializes the executor.
 * @param e The executor.
 * @param module_name The module name.
 * @param sem_settings Semantic settings.
 * @param function_call_require_comma Whether commas are required in function calls.
 */
void executor_init(Executor *e, const char *module_name,
                   const SemanticSettings *sem_settings,
                   int function_call_require_comma);

/**
 * @brief Cleans up the executor.
 * @param e The executor.
 */
void executor_cleanup(Executor *e);

/**
 * @brief Parses source code.
 * @param e The executor.
 * @param source The source string.
 * @param filename The source filename.
 * @param settings Lexer settings.
 * @return The root AST node.
 */
ASTNode* executor_parse_source(Executor *e, const char *source,
                                const char *filename,
                                const LexerSettings *settings);

/**
 * @brief Generates ALIR from an AST.
 * @param e The executor.
 * @param root The root AST node.
 */
void executor_alir_generate(Executor *e, ASTNode *root);

/**
 * @brief Finds a function in the module.
 * @param module The ALIR module.
 * @param name The function name.
 * @return The function, or NULL if not found.
 */
AlirFunction* alir_find_function(AlirModule *module, const char *name);

/**
 * @brief Executes a variable declaration.
 * @param e The executor.
 * @param vd The variable declaration node.
 * @param seq The sequence number.
 * @param prefix The variable name prefix.
 * @return The evaluated value.
 */
long long exec_var_decl(Executor *e, VarDeclNode *vd, int seq, const char *prefix);

/**
 * @brief Executes a class definition.
 * @param e The executor.
 * @param curr The current AST node.
 * @param root The root AST node.
 */
void exec_class(Executor *e, ASTNode *curr, ASTNode *root);

/**
 * @brief Executes a function definition.
 * @param e The executor.
 * @param curr The function definition AST node.
 */
void exec_func_def(Executor *e, ASTNode *curr);

/**
 * @brief Executes a link statement.
 * @param e The executor.
 * @param ln The link node.
 * @param is_repl Whether this is in REPL mode.
 */
void exec_link(Executor *e, LinkNode *ln, int is_repl);

/**
 * @brief Executes an expression.
 * @param e The executor.
 * @param curr The expression AST node.
 * @param seq The sequence number.
 * @param prefix The result name prefix.
 * @param print_rich Whether to print rich output.
 * @param out_type Output parameter for the result type.
 * @return The evaluated value.
 */
long long exec_expr(Executor *e, ASTNode *curr, int seq, const char *prefix,
                    int print_rich, VarType *out_type);

#endif // METARSE_H
