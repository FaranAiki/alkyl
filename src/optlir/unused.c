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
        if (streq(set->names[i], name)) return;
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
        if (streq(set->names[i], name)) return 1;
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

static void check_type_for_struct(Arena *arena, UsedSet *used_structs, VarType *type) {
    if (!type) return;
    if (type->base == TYPE_CLASS && type->class_name) {
        debug_optlir("Check Type for Struct Marking %s\n", type->class_name);
        used_set_add(arena, used_structs, type->class_name);
    }
}

static bool is_struct_used(UsedSet *used_structs, const char *st_name) {
    if (used_set_has(used_structs, st_name)) return true;
    const char *dot = strrchr(st_name, '.');
    if (dot) {
        if (used_set_has(used_structs, dot + 1)) return true;
    }
    return false;
}

static void collect_used_structs(AlirModule *module, UsedSet *used_structs) {
    Arena *arena = module->compiler_ctx ? module->compiler_ctx->arena : NULL;

    AlirGlobal *g = module->globals;
    while (g) {
        check_type_for_struct(arena, used_structs, &g->type);
        g = g->next;
    }

    AlirFunction *f = module->functions;
    while (f) {
        check_type_for_struct(arena, used_structs, &f->ret_type);
        AlirParam *p = f->params;
        while (p) {
            check_type_for_struct(arena, used_structs, &p->type);
            p = p->next;
        }
        AlirBlock *b = f->blocks;
        while (b) {
            AlirInst *i = b->head;
            while (i) {
                if (i->dest) check_type_for_struct(arena, used_structs, &i->dest->type);
                if (i->op1) check_type_for_struct(arena, used_structs, &i->op1->type);
                if (i->op2) check_type_for_struct(arena, used_structs, &i->op2->type);
                for (int j = 0; j < i->arg_count; j++) {
                    if (i->args[j]) check_type_for_struct(arena, used_structs, &i->args[j]->type);
                }
                i = i->next;
            }
            b = b->next;
        }
        f = f->next;
    }

    // Transitive closure for struct fields
    int changed;
    do {
        changed = 0;
        AlirStruct *st = module->structs;
        while (st) {
            if (is_struct_used(used_structs, st->name)) {
                AlirField *field = st->fields;
                while (field) {
                    if (field->type.base == TYPE_CLASS && field->type.class_name && !used_set_has(used_structs, field->type.class_name)) {
                        debug_optlir("ALIR Struct Marking field dep %s\n", field->type.class_name);
                        used_set_add(arena, used_structs, field->type.class_name);
                        changed = 1;
                    }
                    field = field->next;
                }
            }
            st = st->next;
        }
    } while (changed);
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
        if (!is_struct_used(&used_structs, st->name)) {
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
