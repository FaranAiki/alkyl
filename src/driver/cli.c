#include "cli.h"
#include "../common/arena.h"
#include "../common/context.h"
#include "../parser/parser.h"
#include "../semantic/semantic.h"
#include "../alir/alir.h"
#include <meta/vm.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <readline/readline.h>
#include <readline/history.h>

// Global context for autocompletion
static SemanticCtx *global_sem_ctx = NULL;

static char* ethyl_generator(const char* text, int state) {
    static SemSymbol *sym = NULL;
    static int text_len = 0;
    
    if (!state) {
        if (!global_sem_ctx || !global_sem_ctx->global_scope) return NULL;
        sym = global_sem_ctx->global_scope->symbols;
        text_len = strlen(text);
    }
    
    while (sym) {
        char *name = sym->name;
        sym = sym->next; // advance for next call
        
        // Match prefix, or if text is empty match everything (so tab gives all)
        if (text_len == 0 || strncmp(name, text, text_len) == 0) {
            return strdup(name);
        }
    }
    
    return NULL;
}

static char** ethyl_completion(const char* text, int start, int end) {
    (void)start;
    (void)end;
    rl_attempted_completion_over = 1; // Don't fall back to filename completion
    return rl_completion_matches(text, ethyl_generator);
}

static void display_matches_hook(char **matches, int num_matches, int max_length) {
    printf("\033[90m\n"); 
    rl_display_match_list(matches, num_matches, max_length);
    printf("\033[0m"); 
    rl_forced_update_display();
}

char* get_smart_input(Arena* arena, int cmd_count) {
    char prompt[128];
    sprintf(prompt, "\033[32mIn [%d]:\033[0m ", cmd_count);
    
    char *input_buffer = arena_alloc(arena, 4096);
    if (!input_buffer) return NULL;
    input_buffer[0] = '\0';
    int total_len = 0;
    int brace_depth = 0;
    char *line;
    int first_line = 1;
    while (1) {
        if (!first_line) {
            char indent_prompt[128] = "... ";
            for(int i=0; i<brace_depth && i<10; i++) strcat(indent_prompt, "    ");
            line = readline(indent_prompt);
        } else {
            line = readline(prompt);
        }
        if (!line) return NULL;
        if (first_line && strlen(line) > 0) add_history(line);
        int line_len = strlen(line);
        if (total_len + line_len + 2 >= 4096) {
            printf("\033[31mInput too long!\033[0m\n");
            free(line); return NULL;
        }
        strcat(input_buffer, line);
        strcat(input_buffer, " "); 
        total_len += line_len + 1;
        int in_string = 0;
        int in_char = 0;
        for (int i = 0; i < line_len; i++) {
            if (line[i] == '"' && !in_char) in_string = !in_string;
            if (line[i] == '\'' && !in_string) in_char = !in_char;
            if (!in_string && !in_char) {
                if (line[i] == '{') brace_depth++;
                if (line[i] == '}') brace_depth--;
            }
        }
        free(line);
        if (brace_depth <= 0) break;
        first_line = 0;
    }
    return input_buffer;
}

int run_repl(void) {
    printf("\033[36methyl v0.0.1 \033[0m\n");
    printf("Type \033[33m'exit'\033[0m or \033[33m'quit'\033[0m to leave.\n\n");

    rl_attempted_completion_function = ethyl_completion;
    rl_completion_display_matches_hook = display_matches_hook;

    Arena ast_arena;
    arena_init(&ast_arena);

    CompilerContext ctx;
    context_init(&ctx, &ast_arena);

    // Initialize global parser state (we will share the types_map across REPL lines)
    Lexer dummy_l;
    LexerSettings dummy_settings = {0};
    lexer_init(&dummy_l, &ctx, "REPL", "", &dummy_settings);
    Parser p;
    parser_init(&p, &dummy_l, NULL);

    SemanticSettings sem_settings = {0};
    sem_settings.implicit_let = true;
    sem_settings.replace_variable = true;
    SemanticCtx sem;
    sem_init(&sem, &ctx, &sem_settings);
    global_sem_ctx = &sem;

    // We keep one AlirModule appending stuff
    AlirModule *module = alir_create_module(&ctx, "ethyl_repl");
    MetaVM *vm = meta_vm_init();

    // Setup readline autocomplete
    rl_attempted_completion_function = ethyl_completion;
    rl_completion_display_matches_hook = display_matches_hook;

    int cmd_count = 0;

    while (1) {
        char *buffer = get_smart_input(&ast_arena, cmd_count);
        if (!buffer) break; 

        if (strcmp(buffer, "exit ") == 0 || strcmp(buffer, "quit ") == 0) { 
            break;
        }

        int len = strlen(buffer);
        while(len > 0 && buffer[len-1] == ' ') len--;
        buffer[len] = '\0';
        
        if (len > 0 && buffer[len-1] != ';' && buffer[len-1] != '}') {
            if (len + 1 < 4096) strcat(buffer, ";");
        }

        Lexer l;
        LexerSettings settings = {0};
        lexer_init(&l, &ctx, "REPL", buffer, &settings);
        
        p.l = &l;
        p.has_error = 0;
        p.token_pos = 0;
        p.tokens = NULL;
        p.token_capacity = 0;
        p.current_token.type = TOKEN_UNKNOWN;
        
        ASTNode *root = parse_program(&p);
        if (!root || p.has_error) continue;
        
        sem.current_source = buffer;
        sem.current_filename = "REPL";

        sem_scan_top_level(&sem, root);
        ASTNode *curr = root;
        while (curr) {
            if (curr->type == NODE_VAR_DECL) {
                sem_check_var_decl(&sem, (VarDeclNode*)curr, 0);
            } else {
                sem_check_node(&sem, curr);
            }
            curr = curr->next;
        }

        if (ctx.semantic_error_count > 0) {
            ctx.semantic_error_count = 0; // reset for next line
            continue;
        }

        // Evaluate top level expressions using MetaVM
        curr = root;
        while(curr) {
            if (curr->type == NODE_VAR_DECL) {
                VarDeclNode *vd = (VarDeclNode*)curr;
                long long initial_val = 0;
                
                if (vd->initializer) {
                    char func_name[64];
                    sprintf(func_name, "__repl_init_%d", cmd_count);
                    
                    FuncDefNode *fn = arena_alloc(&ast_arena, sizeof(FuncDefNode));
                    fn->base.type = NODE_FUNC_DEF;
                    fn->name = func_name;
                    fn->ret_type = sem_get_node_type(&sem, vd->initializer);
                    fn->has_body = 1;
                    
                    ReturnNode *ret = arena_alloc(&ast_arena, sizeof(ReturnNode));
                    ret->base.type = NODE_RETURN;
                    ret->value = vd->initializer;
                    fn->body = (ASTNode*)ret;
                    
                    sem_check_func_def(&sem, fn);
                    
                    AlirCtx alir_ctx;
                    memset(&alir_ctx, 0, sizeof(AlirCtx));
                    alir_ctx.sem = &sem; 
                    alir_ctx.module = module;
                    
                    alir_gen_function_def(&alir_ctx, fn, NULL);
                    
                    AlirFunction *compiled_fn = module->functions;
                    while (compiled_fn) {
                        if (strcmp(compiled_fn->name, func_name) == 0) break;
                        compiled_fn = compiled_fn->next;
                    }
                    
                    if (compiled_fn) {
                        initial_val = meta_vm_execute(vm, module, compiled_fn, &sem);
                    }
                }
                
                // Add to MetaVM global memory map
                VMGlobal *vg = calloc(1, sizeof(VMGlobal));
                vg->name = strdup(vd->name);
                vg->ptr_val = calloc(1, 8);
                *((long long*)vg->ptr_val) = initial_val;
                vg->next = vm->globals;
                vm->globals = vg;
                
            } else if (curr->type != NODE_FUNC_DEF && curr->type != NODE_CLASS && curr->type != NODE_NAMESPACE) {
                char func_name[64];
                sprintf(func_name, "__repl_expr_%d", cmd_count);
                
                // Wrap in a synthetic function
                FuncDefNode *fn = arena_alloc(&ast_arena, sizeof(FuncDefNode));
                fn->base.type = NODE_FUNC_DEF;
                fn->name = func_name;
                
                VarType ret_type = sem_get_node_type(&sem, curr);
                fn->ret_type = ret_type;
                fn->has_body = 1;
                
                // If it's a statement, we just return nothing or the expression result
                ReturnNode *ret = arena_alloc(&ast_arena, sizeof(ReturnNode));
                ret->base.type = NODE_RETURN;
                ret->value = curr;
                fn->body = (ASTNode*)ret;
                
                sem_check_func_def(&sem, fn);
                
                AlirCtx alir_ctx;
                memset(&alir_ctx, 0, sizeof(AlirCtx));
                alir_ctx.sem = &sem; 
                alir_ctx.module = module;
                
                // Hack: generate the function
                alir_gen_function_def(&alir_ctx, fn, NULL);
                
                AlirFunction *compiled_fn = module->functions;
                while (compiled_fn) {
                    if (strcmp(compiled_fn->name, func_name) == 0) break;
                    compiled_fn = compiled_fn->next;
                }
                
                if (compiled_fn) {
                    long long exit_code = meta_vm_execute(vm, module, compiled_fn, &sem);
                    if (ret_type.base != TYPE_VOID && ret_type.base != TYPE_UNKNOWN) {
                        if ((ret_type.base == TYPE_INT || ret_type.base == TYPE_LONG) && ret_type.ptr_depth == 0 && ret_type.array_size == 0)
                            printf("-> %lld (int)\n", exit_code);
                        else if (ret_type.base == TYPE_SINGLE && ret_type.ptr_depth == 0 && ret_type.array_size == 0) {
                            union { long long i; float f; } u; u.i = exit_code;
                            printf("-> %f (single)\n", (double)u.f);
                        }
                        else if (ret_type.base == TYPE_DOUBLE && ret_type.ptr_depth == 0 && ret_type.array_size == 0) {
                            union { long long i; double d; } u; u.i = exit_code;
                            printf("-> %f (double)\n", u.d);
                        }
                        else if ((ret_type.base == TYPE_CLASS && ret_type.class_name && strcmp(ret_type.class_name, "string") == 0) || (ret_type.base == TYPE_CHAR && ret_type.ptr_depth == 1 && ret_type.array_size == 0))
                            printf("-> %s (char*)\n", (char*)(intptr_t)exit_code);
                        else {
                            if (ret_type.ptr_depth > 0 || ret_type.array_size > 0)
                                printf("-> %p (%s)\n", (void*)(intptr_t)exit_code, sem_type_to_str(ret_type));
                            else
                                printf("-> %lld (%s)\n", exit_code, sem_type_to_str(ret_type));
                        }
                    }
                }
            }
            curr = curr->next;
        }
        
        cmd_count++;
    }
    meta_vm_free(vm);
    sem_cleanup(&sem);
    arena_free(&ast_arena);
    return 0;
}

int main() {
  return run_repl();
}
