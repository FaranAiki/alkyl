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

MetalirRunner* metalir_runner_create(const char *module_name,
                                      const SemanticSettings *sem_settings,
                                      int function_call_require_comma);
void metalir_runner_destroy(MetalirRunner *r);

ASTNode* metalir_parse(MetalirRunner *r, const char *source,
                       const char *filename, const LexerSettings *settings);
void metalir_sem_check(MetalirRunner *r, ASTNode *root);
void metalir_alir_generate(MetalirRunner *r, ASTNode *root);
long long metalir_execute_alir(MetalirRunner *r, const char *func_name);

long long metalir_execute_parse(MetalirRunner *r, ASTNode *root,
                                const char *source, const char *filename);
long long metalir_execute_string(MetalirRunner *r, const char *source,
                                 const char *filename);
int metalir_load_module(MetalirRunner *r, const char *path);
void metalir_resolve_imports(MetalirRunner *r, ASTNode **root);

long long metalir_run_var_decl(MetalirRunner *r, VarDeclNode *vd, int seq);
void metalir_run_class(MetalirRunner *r, ASTNode *curr, ASTNode *root);
void metalir_run_func_def(MetalirRunner *r, ASTNode *curr);
void metalir_run_link(MetalirRunner *r, LinkNode *ln);
long long metalir_run_expr(MetalirRunner *r, ASTNode *curr, int seq,
                           int print_rich, VarType *out_type);
void metalir_print_repl_value(VarType rt, long long result);

#ifdef __cplusplus
}
#endif

#endif
