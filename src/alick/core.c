#include "../../include/alick/alick_internal.h"
#include "../../include/common/diagnostic.h"
#include <stdarg.h>

void alick_error(AlickCtx *ctx, AlirFunction *func, AlirBlock *block, AlirInst *inst, const char *fmt, ...) {
    ctx->error_count++;
    if (ctx->module && ctx->module->compiler_ctx) {
        ctx->module->compiler_ctx->alir_error_count++;
    }

    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (ctx->module->src && inst && inst->line > 0) {
        // Use the powerful diagnostic error reporter for gorgeous error messages linked to source!
        Lexer l;
        lexer_init(&l, ctx->module->compiler_ctx, ctx->module->filename, ctx->module->src);
        
        Token t;
        t.line = inst->line;
        t.col = inst->col;
        t.type = TOKEN_UNKNOWN;
        t.text = NULL;

        // Provide context from the Intermediate Representation
        char extended_msg[2048];
        snprintf(extended_msg, sizeof(extended_msg), "[ALIR: @%s -> %s] %s", 
                 func ? func->name : "global", 
                 block ? block->label : "entry", 
                 msg);

        report_error(&l, t, extended_msg);
    } else {
        // Fallback print for empty blocks or unlinked AST nodes
        fprintf(stderr, "\033[1;31m[Alick Error]\033[0m ");
        
        if (func) fprintf(stderr, "in func '@%s' ", func->name);
        if (block) fprintf(stderr, "block '%s' ", block->label);
        
        fprintf(stderr, "-> %s\n", msg);
        
        if (inst) {
            fprintf(stderr, "  Instruction Context: %s\n", alir_op_str(inst->op));
        }
    }
}

void alick_warning(AlickCtx *ctx, AlirFunction *func, AlirBlock *block, AlirInst *inst, const char *fmt, ...) {
    ctx->warning_count++;
    
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    if (ctx->module->src && inst && inst->line > 0) {
        // Propagate ALIR warnings to the source file!
        Lexer l;
        lexer_init(&l, ctx->module->compiler_ctx, ctx->module->filename, ctx->module->src);
        
        Token t;
        t.line = inst->line;
        t.col = inst->col;
        t.type = TOKEN_UNKNOWN;
        t.text = NULL;

        char extended_msg[2048];
        snprintf(extended_msg, sizeof(extended_msg), "[ALIR: @%s -> %s] %s", 
                 func ? func->name : "global", 
                 block ? block->label : "entry", 
                 msg);

        report_warning(&l, t, extended_msg);
    } else {
        // Fallback Warning
        fprintf(stderr, "\033[1;35m[Alick Warning]\033[0m ");
        
        if (func) fprintf(stderr, "in func '@%s' ", func->name);
        if (block) fprintf(stderr, "block '%s' ", block->label);
        
        fprintf(stderr, "-> %s\n", msg);
    }
}

int alick_check_module(AlirModule *mod) {
    if (!mod) return 0;
    
    AlickCtx ctx;
    ctx.module = mod;
    ctx.error_count = 0;
    ctx.warning_count = 0;

    AlirFunction *func = mod->functions;
    while (func) {
        // Only run checks on defined functions (ignore declarations)
        if (func->block_count > 0) {
            alick_check_cfg(&ctx, func);
            alick_check_types(&ctx, func);
            alick_check_memory(&ctx, func);
        }
        func = func->next;
    }

    if (ctx.error_count > 0) {
        fprintf(stderr, "\033[1;31mALICK Verification Failed:\033[0m %d errors, %d warnings found.\n", 
                ctx.error_count, ctx.warning_count);
    } else {
        // Optional success info
        // fprintf(stdout, "\033[1;32mALICK Verification Passed.\033[0m\n");
    }

    return ctx.error_count;
}
