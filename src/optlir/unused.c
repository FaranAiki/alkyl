#include "optlir.h"
#include "common/arena.h"
#include <string.h>
#include <ctype.h>

static void used_set_init(UsedSet *set) {
    set->names = NULL;
    set->count = 0;
    set->capacity = 0;
}

static void used_set_add(Arena *arena, UsedSet *set, const char *name) {
    if (!name) return;
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->names[i], name) == 0) return;
    }
    if (set->count >= set->capacity) {
        int new_cap = set->capacity ? set->capacity * 2 : 16;
        char **new_names = arena_alloc(arena, sizeof(char *) * new_cap);
        if (set->names) {
            memcpy(new_names, set->names, sizeof(char *) * set->count);
        }
        set->names = new_names;
        set->capacity = new_cap;
    }
    set->names[set->count++] = arena_strdup(arena, name);
}

static int used_set_has(const UsedSet *set, const char *name) {
    if (!name) return 0;
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->names[i], name) == 0) return 1;
    }
    return 0;
}

static void used_set_free(UsedSet *set) {
    set->names = NULL;
    set->count = 0;
    set->capacity = 0;
}

static void collect_called_functions(AlirModule *module, UsedSet *called) {
    Arena *arena = module->compiler_ctx ? module->compiler_ctx->arena : NULL;
    AlirFunction *f = module->functions;
    while (f) {
        AlirBlock *b = f->blocks;
        while (b) {
            AlirInst *i = b->head;
            while (i) {
                if (i->op == ALIR_OP_CALL && i->op1) {
                    if (i->op1->kind == ALIR_VAL_VAR && i->op1->val.str_val) {
                        used_set_add(arena, called, i->op1->val.str_val);
                    }
                    if (i->op1->kind == ALIR_VAL_GLOBAL && i->op1->val.str_val) {
                        used_set_add(arena, called, i->op1->val.str_val);
                    }
                }
                if (i->op == ALIR_OP_STORE && i->op1) {
                    if (i->op1->kind == ALIR_VAL_VAR && i->op1->val.str_val &&
                        i->op1->val.str_val[0] != 'p' && !isdigit((unsigned char)i->op1->val.str_val[0])) {
                        used_set_add(arena, called, i->op1->val.str_val);
                    }
                    if (i->op1->kind == ALIR_VAL_GLOBAL && i->op1->val.str_val) {
                        used_set_add(arena, called, i->op1->val.str_val);
                    }
                }
                i = i->next;
            }
            b = b->next;
        }
        f = f->next;
    }
}

static void collect_used_structs(AlirModule *module, UsedSet *used_structs) {
    Arena *arena = module->compiler_ctx ? module->compiler_ctx->arena : NULL;
    AlirFunction *f = module->functions;
    while (f) {
        if (f->ret_type.base == TYPE_CLASS && f->ret_type.class_name) {
            used_set_add(arena, used_structs, f->ret_type.class_name);
        }
        AlirParam *p = f->params;
        while (p) {
            if (p->type.base == TYPE_CLASS && p->type.class_name) {
                used_set_add(arena, used_structs, p->type.class_name);
            }
            p = p->next;
        }
        AlirBlock *b = f->blocks;
        while (b) {
            AlirInst *i = b->head;
            while (i) {
                if (i->dest && i->dest->type.base == TYPE_CLASS && i->dest->type.class_name) {
                    used_set_add(arena, used_structs, i->dest->type.class_name);
                }
                if (i->op1 && i->op1->type.base == TYPE_CLASS && i->op1->type.class_name) {
                    used_set_add(arena, used_structs, i->op1->type.class_name);
                }
                if (i->op2 && i->op2->type.base == TYPE_CLASS && i->op2->type.class_name) {
                    used_set_add(arena, used_structs, i->op2->type.class_name);
                }
                for (int j = 0; j < i->arg_count; j++) {
                    if (i->args[j] && i->args[j]->type.base == TYPE_CLASS &&
                        i->args[j]->type.class_name) {
                        used_set_add(arena, used_structs, i->args[j]->type.class_name);
                    }
                }
                i = i->next;
            }
            b = b->next;
        }
        f = f->next;
    }
}

static void collect_used_globals(AlirModule *module, UsedSet *used_globals) {
    Arena *arena = module->compiler_ctx ? module->compiler_ctx->arena : NULL;
    AlirFunction *f = module->functions;
    while (f) {
        AlirBlock *b = f->blocks;
        while (b) {
            AlirInst *i = b->head;
            while (i) {
                if (i->op1 && i->op1->kind == ALIR_VAL_GLOBAL && i->op1->val.str_val) {
                    used_set_add(arena, used_globals, i->op1->val.str_val);
                }
                if (i->op2 && i->op2->kind == ALIR_VAL_GLOBAL && i->op2->val.str_val) {
                    used_set_add(arena, used_globals, i->op2->val.str_val);
                }
                if (i->op == ALIR_OP_CALL && i->op1 && i->op1->kind == ALIR_VAL_GLOBAL &&
                    i->op1->val.str_val) {
                    used_set_add(arena, used_globals, i->op1->val.str_val);
                }
                for (int j = 0; j < i->arg_count; j++) {
                    if (i->args[j] && i->args[j]->kind == ALIR_VAL_GLOBAL &&
                        i->args[j]->val.str_val) {
                        used_set_add(arena, used_globals, i->args[j]->val.str_val);
                    }
                }
                i = i->next;
            }
            b = b->next;
        }
        f = f->next;
    }
}

static void build_used_function_set(AlirModule *module, UsedSet *used_funcs) {
    Arena *arena = module->compiler_ctx ? module->compiler_ctx->arena : NULL;
    UsedSet called;
    used_set_init(&called);
    collect_called_functions(module, &called);

    AlirFunction *f = module->functions;
    while (f) {
        if (used_set_has(&called, f->name)) {
            used_set_add(arena, used_funcs, f->name);
            if (f->is_flux) {
                char resume_name[512];
                snprintf(resume_name, sizeof(resume_name), "%s_Resume", f->name);
                used_set_add(arena, used_funcs, resume_name);
            }
        }
        f = f->next;
    }

    used_set_add(arena, used_funcs, "main");
    used_set_free(&called);
}

void optlir_remove_unused_function(AlirModule *module) {
    UsedSet used_funcs;
    used_set_init(&used_funcs);
    build_used_function_set(module, &used_funcs);

    AlirFunction **funcs = &module->functions;
    while (*funcs) {
        AlirFunction *f = *funcs;
        if (!used_set_has(&used_funcs, f->name)) {
            *funcs = f->next;
            continue;
        }
        funcs = &f->next;
    }

    used_set_free(&used_funcs);
}

void optlir_remove_unused_struct(AlirModule *module) {
    UsedSet used_structs;
    used_set_init(&used_structs);
    collect_used_structs(module, &used_structs);

    AlirStruct **structs = &module->structs;
    while (*structs) {
        AlirStruct *st = *structs;
        if (!used_set_has(&used_structs, st->name)) {
            *structs = st->next;
            continue;
        }
        structs = &st->next;
    }

    used_set_free(&used_structs);
}

void optlir_remove_unused_variable(AlirModule *module) {
    UsedSet used_globals;
    used_set_init(&used_globals);
    collect_used_globals(module, &used_globals);

    AlirGlobal **globals = &module->globals;
    while (*globals) {
        AlirGlobal *g = *globals;
        if (!used_set_has(&used_globals, g->name)) {
            *globals = g->next;
            continue;
        }
        globals = &g->next;
    }

    used_set_free(&used_globals);
}

void optlir_remove_unused(AlirModule *module) {
    optlir_remove_unused_function(module);
    optlir_remove_unused_struct(module);
    optlir_remove_unused_variable(module);
}
