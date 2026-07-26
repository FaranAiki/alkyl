#ifndef METARSE_H
#define METARSE_H

#include "../common/arena.h"
#include "../common/context.h"
#include "../parser/parser.h"
#include "../semantic/semantic.h"
#include "../alir/alir.h"
#include <metalir/vm.h>

typedef struct {
    Arena ast_arena;
    CompilerContext ctx;
    Parser p;
    SemanticCtx sem;
    AlirModule *module;
    Arena vm_arena;
    MetaVM *vm;
} Executor;

void executor_init(Executor *e, const char *module_name,
                   const SemanticSettings *sem_settings,
                   int function_call_require_comma);
void executor_cleanup(Executor *e);
ASTNode* executor_parse_source(Executor *e, const char *source,
                              const char *filename,
                              const LexerSettings *settings);
void executor_alir_generate(Executor *e, ASTNode *root);
AlirFunction* alir_find_function(AlirModule *module, const char *name);

long long exec_var_decl(Executor *e, VarDeclNode *vd, int seq, const char *prefix);
void exec_class(Executor *e, ASTNode *curr, ASTNode *root);
void exec_func_def(Executor *e, ASTNode *curr);
void exec_link(Executor *e, LinkNode *ln, int is_repl);
long long exec_expr(Executor *e, ASTNode *curr, int seq, const char *prefix,
                    int print_rich, VarType *out_type);

#endif // METARSE_H
