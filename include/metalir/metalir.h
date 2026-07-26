#ifndef METALIR_H
#define METALIR_H

#include "parser/parser.h"
#include "semantic/semantic.h"
#include "alir/alir.h"

#include "common/arena.h"
#include "common/context.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MetalirVM MetalirVM;

typedef struct {
    Arena ast_arena;
    CompilerContext ctx;
    Parser parser;
    SemanticCtx sem;
    AlirModule *module;
    Arena vm_arena;
    MetalirVM *vm;
} MetalirState;

void metalir_state_init(MetalirState *state, const char *module_name,
                        const SemanticSettings *sem_settings,
                        int function_call_require_comma);
void metalir_state_cleanup(MetalirState *state);

ASTNode* metalir_parse(MetalirState *state, const char *source,
                       const char *filename, const LexerSettings *settings);
void metalir_sem_check(MetalirState *state, ASTNode *root);
void metalir_alir_generate(MetalirState *state, ASTNode *root);
long long metalir_execute_alir(MetalirState *state, const char *func_name);

long long metalir_execute_parse(MetalirState *state, ASTNode *root,
                                const char *source, const char *filename);
long long metalir_execute_string(MetalirState *state, const char *source,
                                 const char *filename);

#ifdef __cplusplus
}
#endif

#endif
