#include "semantic.h"
#include "common/hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

unsigned int hash_ptr(ASTNode *node) {
    uintptr_t ptr_val = (uintptr_t)node;
    return (unsigned int)((ptr_val >> 3) % TYPE_TABLE_SIZE);
}

void sem_set_node_type(SemanticCtx *ctx, ASTNode *node, VarType type) {
    if (!node) return;
    node->sem_type = type;
    unsigned int idx = hash_ptr(node);

    TypeEntry *curr = ctx->type_buckets[idx];
    while (curr) {
        if (curr->node == node) {
            if (type.base != TYPE_UNKNOWN) {
                curr->type = type;
            }
            return;
        }
        curr = curr->next;
    }

    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return;

    TypeEntry *entry = arena_alloc_type(ctx->compiler_ctx->arena, TypeEntry);
    entry->node = node;
    entry->type = type;
    entry->is_tainted = 0;
    entry->is_impure = 0;
    entry->next = ctx->type_buckets[idx];
    ctx->type_buckets[idx] = entry;
}

VarType sem_get_node_type(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};

    unsigned int idx = hash_ptr(node);
    TypeEntry *curr = ctx->type_buckets[idx];
    while (curr) {
        if (curr->node == node && curr->type.base != TYPE_UNKNOWN) return curr->type;
        curr = curr->next;
    }

    if (node->sem_type.base != TYPE_UNKNOWN) {
        return node->sem_type;
    }

    return (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
}

void sem_set_node_tainted(SemanticCtx *ctx, ASTNode *node, int is_tainted) {
    if (!node) return;
    unsigned int idx = hash_ptr(node);
    TypeEntry *curr = ctx->type_buckets[idx];
    while (curr) {
        if (curr->node == node) {
            curr->is_tainted = is_tainted;
            return;
        }
        curr = curr->next;
    }

    sem_set_node_type(ctx, node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    ctx->type_buckets[idx]->is_tainted = is_tainted;
}

int sem_get_node_tainted(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return 0;
    unsigned int idx = hash_ptr(node);
    TypeEntry *curr = ctx->type_buckets[idx];
    while (curr) {
        if (curr->node == node) return curr->is_tainted;
        curr = curr->next;
    }
    return 0;
}

void sem_set_node_impure(SemanticCtx *ctx, ASTNode *node, int is_impure) {
    if (!node) return;
    unsigned int idx = hash_ptr(node);
    TypeEntry *curr = ctx->type_buckets[idx];
    while (curr) {
        if (curr->node == node) {
            curr->is_impure = is_impure;
            return;
        }
        curr = curr->next;
    }

    sem_set_node_type(ctx, node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    ctx->type_buckets[idx]->is_impure = is_impure;
}

int sem_get_node_impure(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return 0;
    unsigned int idx = hash_ptr(node);
    TypeEntry *curr = ctx->type_buckets[idx];
    while (curr) {
        if (curr->node == node) return curr->is_impure;
        curr = curr->next;
    }
    return 0;
}

SemSymbol* find_in_scope_direct(SemScope *scope, const char *name) {
    if (scope->symbol_map) {
        SemSymbol *res = (SemSymbol*)hashmap_get((HashMap*)scope->symbol_map, name);
        if (res && res->kind == SYM_FUNC && scope->is_class_scope && scope->class_sym && res->name == scope->class_sym->name) {
            return NULL; // Do not return the constructor here to avoid shadowing the class name
        }
        return res;
    }
    // Fallback if hashmap is not initialized
    SemSymbol *sym = scope->symbols;
    while (sym) {
        if (streq_lit(sym->name, name)) {
            if (sym->kind == SYM_FUNC && scope->is_class_scope && scope->class_sym && streq_lit(sym->name, scope->class_sym->name)) {
                // skip constructor
            } else {
                return sym;
            }
        }
        sym = sym->next;
    }
    return NULL;
}

void sem_init(SemanticCtx *ctx, CompilerContext *compiler_ctx, SemanticSettings *settings) {
    ctx->compiler_ctx = compiler_ctx;
    if (settings) {
        ctx->settings = *settings;
    } else {
        ctx->settings.implicit_let = false;
        ctx->settings.replace_variable = false;
        ctx->settings.namespace_auto_search = true;
        ctx->settings.namespace_ausearch_warning = true;
    }

    if (compiler_ctx && compiler_ctx->arena) {
        ctx->global_scope = arena_alloc_type(compiler_ctx->arena, SemScope);
        memset(ctx->global_scope, 0, sizeof(SemScope));
        ctx->global_scope->symbol_map = arena_alloc_type(compiler_ctx->arena, HashMap);
        hashmap_init((HashMap*)ctx->global_scope->symbol_map, compiler_ctx->arena, 64);
    }

    ctx->current_scope = ctx->global_scope;
    ctx->current_func_sym = NULL;
    ctx->in_wash_block = 0;
    ctx->in_loop = 0;
    ctx->in_switch = 0;
    ctx->current_source = NULL;
    ctx->current_filename = NULL;

    for (int i = 0; i < TYPE_TABLE_SIZE; i++) {
        ctx->type_buckets[i] = NULL;
    }
}

void sem_cleanup(SemanticCtx *ctx) {
    ctx->current_scope = NULL;
    ctx->global_scope = NULL;
    ctx->current_func_sym = NULL;
    for (int i = 0; i < TYPE_TABLE_SIZE; i++) {
        ctx->type_buckets[i] = NULL;
    }
}

void sem_scope_enter(SemanticCtx *ctx, int is_func, VarType ret_type) {
    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return;

    SemScope *new_scope = arena_alloc_type(ctx->compiler_ctx->arena, SemScope);
    memset(new_scope, 0, sizeof(SemScope));

    new_scope->symbols = NULL;
    new_scope->symbol_map = arena_alloc_type(ctx->compiler_ctx->arena, HashMap);
    hashmap_init((HashMap*)new_scope->symbol_map, ctx->compiler_ctx->arena, 16);
    new_scope->parent = ctx->current_scope;
    if (is_func) {
        new_scope->is_function_scope = 1;
        new_scope->expected_ret_type = ret_type;
    } else if (ctx->current_scope) {
        new_scope->is_function_scope = ctx->current_scope->is_function_scope;
        new_scope->expected_ret_type = ctx->current_scope->expected_ret_type;
    } else {
        new_scope->is_function_scope = 0;
        new_scope->expected_ret_type = ret_type;
    }
    new_scope->is_class_scope = 0;

    ctx->current_scope = new_scope;
}

void sem_scope_exit(SemanticCtx *ctx) {
    if (ctx->current_scope->parent) {
        ctx->current_scope = ctx->current_scope->parent;
    }
}

SemSymbol* sem_symbol_add(SemanticCtx *ctx, const char *name, SymbolKind kind, VarType type) {
    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return NULL;

    SemSymbol *sym = arena_alloc_type(ctx->compiler_ctx->arena, SemSymbol);
    memset(sym, 0, sizeof(SemSymbol));

    sym->name = arena_strdup(ctx->compiler_ctx->arena, name);
    sym->kind = kind;
    sym->type = type;
    sym->params = NULL;
    sym->param_count = 0;
    sym->parent_name = NULL;
    sym->is_variadic = false;
    sym->is_mutable = true;
    sym->is_initialized = true;
    sym->is_pure = false; // default to false
    sym->must_pure = false; // default to false
    sym->is_pristine = true;
    sym->must_pristine = false;
    sym->inner_scope = NULL;

    if (ctx->current_scope) {
        sym->next = ctx->current_scope->symbols;
        ctx->current_scope->symbols = sym;

        if (ctx->current_scope->symbol_map) {
            SemSymbol *existing = hashmap_get((HashMap*)ctx->current_scope->symbol_map, sym->name);
            if (existing && existing->kind == SYM_FUNC && sym->kind == SYM_FUNC) {
                // printf("Adding overload %s to existing %s\n", sym->mangled_name, existing->mangled_name);
                SemSymbol *last = existing;
                while (last->overload_next) last = last->overload_next;
                last->overload_next = sym;
            } else {
                // printf("Adding new symbol %s\n", sym->name);
                hashmap_put((HashMap*)ctx->current_scope->symbol_map, sym->name, sym);
            }
        }
    } else {
        sym->next = ctx->global_scope->symbols;
        ctx->global_scope->symbols = sym;

        if (ctx->global_scope->symbol_map) {
            SemSymbol *existing = hashmap_get((HashMap*)ctx->global_scope->symbol_map, sym->name);
            if (existing && existing->kind == SYM_FUNC && sym->kind == SYM_FUNC) {
                // printf("Adding overload %s to existing %s\n", sym->mangled_name, existing->mangled_name);
                SemSymbol *last = existing;
                while (last->overload_next) last = last->overload_next;
                last->overload_next = sym;
            } else {
                // printf("Adding new symbol %s\n", sym->name);
                hashmap_put((HashMap*)ctx->global_scope->symbol_map, sym->name, sym);
            }
        }
    }

    return sym;
}

SemSymbol* sem_symbol_lookup_type(SemanticCtx *ctx, const char *name) {
    if (!name) return NULL;
    const char *dot = strchr(name, '.');
    if (dot) {
        char base_name[256];
        int len = dot - name;
        if (len >= (int)sizeof(base_name)) len = (int)sizeof(base_name) - 1;
        strncpy(base_name, name, len);
        base_name[len] = '\0';

        SemSymbol *base_sym = sem_symbol_lookup_type(ctx, base_name);
        if (base_sym && base_sym->inner_scope) {
            SemScope *old = ctx->current_scope;
            ctx->current_scope = base_sym->inner_scope;
            SemSymbol *res = sem_symbol_lookup_type(ctx, dot + 1);
            ctx->current_scope = old;
            return res;
        }
        return NULL;
    }
    SemScope *scope = ctx->current_scope;
    while (scope) {
        SemSymbol *sym = find_in_scope_direct(scope, name);
        if (sym && (sym->kind == SYM_CLASS || sym->kind == SYM_ENUM || sym->kind == SYM_NAMESPACE || sym->kind == SYM_TEMPLATE)) {
            if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0)) {
                return sym;
            }
        }

        // If we found a sym but it's not a type (e.g. constructor), keep searching upwards
        scope = scope->parent;
    }

    // Check global scope directly if not reached
    SemSymbol *sym = find_in_scope_direct(ctx->global_scope, name);
    if (sym && (sym->kind == SYM_CLASS || sym->kind == SYM_ENUM || sym->kind == SYM_NAMESPACE || sym->kind == SYM_TEMPLATE)) {
        if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0)) {
            return sym;
        }
    }
    if (ctx->settings.namespace_auto_search) {
        SemSymbol *ns = ctx->global_scope->symbols;
        while (ns) {
            if (ns->kind == SYM_NAMESPACE && ns->inner_scope) {
                SemSymbol *sym = find_in_scope_direct(ns->inner_scope, name);
                if (sym && (sym->kind == SYM_CLASS || sym->kind == SYM_ENUM || sym->kind == SYM_NAMESPACE || sym->kind == SYM_TEMPLATE)) {
                    if (sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0) {
                        continue;
                    }
                    if (ctx->settings.namespace_ausearch_warning) {
                        const char *current_ns = ctx->compiler_ctx ? diag_get_namespace(ctx->compiler_ctx) : NULL;
                        if (!current_ns || !streq_lit(current_ns, ns->name)) {
                            if (ctx->current_node) {
                                sem_warning(ctx, ctx->current_node, "Implicitly resolved '%s' to '%s.%s'", name, ns->name, name);
                                sem_hint(ctx, ctx->current_node, "consider writing %s.%s", ns->name, name);
                            } else {
                                sem_warning(ctx, NULL, "Implicitly resolved '%s' to '%s.%s'", name, ns->name, name);
                                sem_hint(ctx, NULL, "consider writing %s.%s", ns->name, name);
                            }
                        }
                    }
                    return sym;
                }
            }
            ns = ns->next;
        }
    }

    return NULL;
}

SemSymbol* sem_symbol_lookup(SemanticCtx *ctx, const char *name, SemScope **out_scope) {
    if (!name) return NULL;
    const char *dot = strchr(name, '.');
    if (dot) {
        char base_name[256];
        int len = dot - name;
        if (len >= (int)sizeof(base_name)) len = (int)sizeof(base_name) - 1;
        strncpy(base_name, name, len);
        base_name[len] = '\0';

        SemScope *found_scope = NULL;
        SemSymbol *base_sym = sem_symbol_lookup(ctx, base_name, &found_scope);
        if (base_sym && base_sym->inner_scope) {
            SemScope *old = ctx->current_scope;
            ctx->current_scope = base_sym->inner_scope;
            SemSymbol *res = sem_symbol_lookup(ctx, dot + 1, out_scope);
            ctx->current_scope = old;
            return res;
        }
        // If the base symbol was found but has no inner_scope (e.g., a function
        // shadows a namespace), continue searching in parent scopes for a
        // namespace/class base symbol that can resolve the dotted member.
        if (base_sym && !base_sym->inner_scope && found_scope) {
            SemScope *parent_scope = found_scope->parent;
            while (parent_scope) {
                SemSymbol *parent_sym = find_in_scope_direct(parent_scope, base_name);
                if (parent_sym && parent_sym->inner_scope) {
                    SemScope *old = ctx->current_scope;
                    ctx->current_scope = parent_sym->inner_scope;
                    SemSymbol *res = sem_symbol_lookup(ctx, dot + 1, out_scope);
                    ctx->current_scope = old;
                    return res;
                }
                parent_scope = parent_scope->parent;
            }
        }
        return NULL;
    }
    SemScope *scope = ctx->current_scope;
    while (scope) {
        SemSymbol *sym = find_in_scope_direct(scope, name);
        if (sym) {
            if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0)) {
                if (out_scope) *out_scope = scope;
                return sym;
            }
        }

        sym = scope->symbols;
        while (sym) {
            if (sym->kind == SYM_ENUM && sym->inner_scope) {
                SemSymbol *mem = sym->inner_scope->symbols;
                while (mem) {
                    if (streq_lit(mem->name, name)) {
                        if (out_scope) *out_scope = sym->inner_scope;
                        return mem;
                    }
                    mem = mem->next;
                }
            }
            sym = sym->next;
        }

        if (scope->is_class_scope && scope->class_sym && scope->class_sym->parent_name) {
            SemScope *search_scope = scope->parent;
            SemSymbol *parent_class = NULL;

            while (search_scope) {
                parent_class = find_in_scope_direct(search_scope, scope->class_sym->parent_name);
                if (parent_class && parent_class->kind == SYM_CLASS) break;
                search_scope = search_scope->parent;
                parent_class = NULL;
            }

            if (parent_class && parent_class->inner_scope) {
                SemSymbol *inherited = find_in_scope_direct(parent_class->inner_scope, name);
                if (inherited) {
                    if (out_scope) *out_scope = parent_class->inner_scope;
                    return inherited;
                }

                SemSymbol *curr_cls = parent_class;
                while (curr_cls && curr_cls->parent_name) {
                    SemScope *gp_search = ctx->global_scope;
                    SemSymbol *grandparent = find_in_scope_direct(gp_search, curr_cls->parent_name);

                    if (grandparent && grandparent->kind == SYM_CLASS && grandparent->inner_scope) {
                         inherited = find_in_scope_direct(grandparent->inner_scope, name);
                         if (inherited) {
                             if (out_scope) *out_scope = grandparent->inner_scope;
                             return inherited;
                         }
                         curr_cls = grandparent;
                    } else {
                        break;
                    }
                }
            }
        }

        scope = scope->parent;
    }
    if (ctx->settings.namespace_auto_search) {
        SemSymbol *ns = ctx->global_scope->symbols;
        while (ns) {
            if (ns->kind == SYM_NAMESPACE && ns->inner_scope) {
                SemSymbol *sym = find_in_scope_direct(ns->inner_scope, name);
                if (sym) {
                    if (sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0) {
                        continue;
                    }
                    if (ctx->settings.namespace_ausearch_warning) {
                        const char *current_ns = ctx->compiler_ctx ? diag_get_namespace(ctx->compiler_ctx) : NULL;
                        debug_semantic("table.c lookup: name='%s', found_ns='%s', current_ns='%s'\n", name, ns->name, current_ns ? current_ns : "(null)");
                        if (!current_ns || !streq_lit(current_ns, ns->name)) {
                            if (ctx->current_node) {
                                sem_warning(ctx, ctx->current_node, "Implicitly resolved '%s' to '%s.%s'", name, ns->name, name);
                                sem_hint(ctx, ctx->current_node, "consider writing %s.%s", ns->name, name);
                            } else {
                                sem_warning(ctx, NULL, "Implicitly resolved '%s' to '%s.%s'", name, ns->name, name);
                                sem_hint(ctx, NULL, "consider writing %s.%s", ns->name, name);
                            }
                        }
                    }
                    if (out_scope) *out_scope = ns->inner_scope;
                    return sym;
                }
            }
            ns = ns->next;
        }
    }

    if (out_scope) *out_scope = NULL;
    return NULL;
}

int sem_types_are_equal(VarType a, VarType b) {
    if (a.base != b.base) return 0;
    if (a.array_size != b.array_size) return 0;
    if (a.array_depth != b.array_depth) return 0;
    if (a.ptr_depth != b.ptr_depth) return 0;
    if (a.is_unsigned != b.is_unsigned) return 0;

    if (a.base == TYPE_CLASS || a.base == TYPE_ENUM || a.base == TYPE_NAMESPACE) {
        if (a.class_name && b.class_name) {
            if (streq_lit(a.class_name, b.class_name)) return 1;

            char mangled_a[512] = "";
            char mangled_b[512] = "";

            for (int k = 0; k < 2; k++) {
                const char *src = (k == 0) ? a.class_name : b.class_name;
                char *dst = (k == 0) ? mangled_a : mangled_b;

                char tmp[512];
                snprintf(tmp, sizeof(tmp), "%s", src);
                if (strchr(tmp, '[')) {
                    for (int i=0; tmp[i]; i++) {
                        if (tmp[i] == '[') tmp[i] = '_';
                        else if (tmp[i] == ']') tmp[i] = '\0';
                        else if (tmp[i] == ',' || tmp[i] == ' ') tmp[i] = '_';
                    }
                    int j = 0;
                    for (int i=0; tmp[i] && j < 511; i++) {
                        if (tmp[i] == '_' && tmp[i+1] == '_') continue;
                        dst[j++] = tmp[i];
                    }
                    dst[j] = '\0';
                } else {
                    snprintf(dst, 512, "%s", src);
                }
            }

            if (streq_lit(mangled_a, mangled_b)) return 1;

            const char *dot_a = strrchr(mangled_a, '.');
            const char *dot_b = strrchr(mangled_b, '.');
            const char *base_a = dot_a ? dot_a + 1 : mangled_a;
            const char *base_b = dot_b ? dot_b + 1 : mangled_b;
            return streq_lit(base_a, base_b);
        }
        return 0;
    }
    return 1;
}

/* TODO fix this for implicit casting */
bool sem_types_are_compatible(SemanticCtx *ctx, VarType dest, VarType src) {
    if (dest.base == TYPE_CLASS && src.base == TYPE_CLASS && dest.class_name && src.class_name) {
        if (dest.ptr_depth == src.ptr_depth) {
            if (streq_lit(dest.class_name, src.class_name)) return true;

            // Check inheritance
            SemSymbol *src_sym = sem_symbol_lookup(ctx, src.class_name, NULL);
            while (src_sym && src_sym->kind == SYM_CLASS && src_sym->parent_name) {
                if (streq_lit(src_sym->parent_name, dest.class_name)) return true;
                src_sym = sem_symbol_lookup(ctx, src_sym->parent_name, NULL);
            }

            // Check traits
            src_sym = sem_symbol_lookup(ctx, src.class_name, NULL);
            while (src_sym && src_sym->kind == SYM_CLASS) {
                for (int i = 0; i < src_sym->trait_count; i++) {
                    if (streq_lit(src_sym->traits[i], dest.class_name)) return true;

                    // Recursive trait check (traits can have traits)
                    SemSymbol *trait_sym = sem_symbol_lookup(ctx, src_sym->traits[i], NULL);
                    if (trait_sym && trait_sym->kind == SYM_CLASS) {
                         // TODO: should we check transitively?
                    }
                }
                if (src_sym->parent_name) {
                    src_sym = sem_symbol_lookup(ctx, src_sym->parent_name, NULL);
                } else {
                    break;
                }
            }
        }
    }

    if (sem_types_are_equal(dest, src)) return true;

    if (dest.base == TYPE_AUTO) return true;

    if (dest.base == TYPE_CLASS && dest.class_name && streq_lit(dest.class_name, "string") && src.base == TYPE_CHAR) return true;
    if (src.base == TYPE_CLASS && src.class_name && streq_lit(src.class_name, "string") && dest.base == TYPE_CHAR) return true; // Adding this just in case
    if (dest.base == TYPE_CHAR && src.base == TYPE_CHAR && (dest.ptr_depth > 0 || dest.array_size > 0) && (src.ptr_depth > 0 || src.array_size > 0)) return true;

    // TODO make this more proper!
    if ((dest.base == TYPE_VOID /*&& dest.ptr_depth > 0*/) || (src.base == TYPE_VOID /*&& src.ptr_depth > 0*/)) return true;

    // TODO: fix this
    int dest_is_num = (dest.base >= TYPE_INT && dest.base <= TYPE_LONG_DOUBLE);
    int src_is_num = (src.base >= TYPE_INT && src.base <= TYPE_LONG_DOUBLE);

    if (src.base == TYPE_ENUM && dest_is_num) return 1;
    if (dest.base == TYPE_ENUM && src_is_num) return 1;

    if (dest_is_num && src_is_num && dest.ptr_depth == 0 && src.ptr_depth == 0 && dest.array_size == 0 && src.array_size == 0) {
        return true;
    }

    int dest_is_str = (dest.base == TYPE_CLASS && dest.class_name && streq_lit(dest.class_name, "string") && dest.ptr_depth == 0);
    int src_is_str = (src.base == TYPE_CLASS && src.class_name && streq_lit(src.class_name, "string") && src.ptr_depth == 0);

    int dest_is_char_p = (dest.base == TYPE_CHAR && (dest.ptr_depth > 0 || dest.array_size > 0));
    int src_is_char_p = (src.base == TYPE_CHAR && (src.ptr_depth > 0 || src.array_size > 0));

    if ((dest_is_str && src_is_char_p) || (dest_is_char_p && src_is_str)) {
        return true;
    }

    if (src.array_size > 0 && dest.ptr_depth == src.ptr_depth + 1 && dest.base == src.base) {
        return true;
    }


    // casting char*, int* to void* or int* to void*
    if ((dest.base == TYPE_VOID && dest.ptr_depth > 0 && src.ptr_depth > 0) || (src.base == TYPE_VOID && src.ptr_depth > 0 && dest.ptr_depth > 0)) return true;

    // We shouldn't allow implicit casting between pointers and integers (except maybe for 0, but that needs to be handled via literals)

    return false;
}

bool sem_types_are_castable(SemanticCtx *ctx, VarType dest, VarType src) {
    if (sem_types_are_compatible(ctx, dest, src)) return true;
    
    int dest_is_num = (dest.base >= TYPE_INT && dest.base <= TYPE_LONG_DOUBLE);
    int src_is_num = (src.base >= TYPE_INT && src.base <= TYPE_LONG_DOUBLE);
    
    // Explicit casts between pointer and integer
    if (dest_is_num && dest.ptr_depth == 0 && src.ptr_depth > 0) return true;
    if (src_is_num && src.ptr_depth == 0 && dest.ptr_depth > 0) return true;
    
    // Explicit casts between pointer and pointer (handled by void* implicitly usually, but explicit cast allows any pointer to any pointer)
    if (dest.ptr_depth > 0 && src.ptr_depth > 0) return true;
    
    return false;
}

static void sem_type_to_str_rec(VarType t, char *buf, int max_len, int *pos) {
    if (*pos >= max_len - 1) return;

    if (t.is_func_ptr) {
        if (t.fp_ret_type) {
            sem_type_to_str_rec(*t.fp_ret_type, buf, max_len, pos);
        } else {
            *pos += snprintf(buf + *pos, max_len - *pos, "void");
        }
        *pos += snprintf(buf + *pos, max_len - *pos, "(");
        if (t.fp_param_count > 0 || t.fp_is_varargs) {
            for (int i=0; i<t.fp_param_count; i++) {
                if (i > 0) *pos += snprintf(buf + *pos, max_len - *pos, ", ");
                sem_type_to_str_rec(t.fp_param_types[i], buf, max_len, pos);
            }
            if (t.fp_is_varargs) {
                if (t.fp_param_count > 0) *pos += snprintf(buf + *pos, max_len - *pos, ", ");
                *pos += snprintf(buf + *pos, max_len - *pos, "...");
            }
        }
        *pos += snprintf(buf + *pos, max_len - *pos, ")");
        return;
    }

    const char *base = "unknown";
    switch(t.base) {
        case TYPE_INT: base = "int"; break;
        case TYPE_UNSIGNED_INT: base = "unsigned int"; break;
        case TYPE_SHORT: base = "short"; break;
        case TYPE_LONG: base = "long"; break;
        case TYPE_UNSIGNED_LONG: base = "unsigned long"; break;
        case TYPE_LONG_LONG: base = "long long"; break;
        case TYPE_UNSIGNED_LONG_LONG: base = "unsigned long long"; break;
        case TYPE_CHAR: base = "char"; break;
        case TYPE_UNSIGNED_CHAR: base = "unsigned char"; break;
        case TYPE_BOOL: base = "bool"; break;
        case TYPE_SINGLE: base = "single"; break;
        case TYPE_DOUBLE: base = "double"; break;
        case TYPE_LONG_DOUBLE: base = "long double"; break;
        case TYPE_VOID: base = "void"; break;
        case TYPE_ERROR: base = "error"; break;
        case TYPE_ARRAY: base = "array"; break;
        case TYPE_AUTO: base = "let"; break;
        case TYPE_CLASS: base = t.class_name ? t.class_name : "class"; break;
        case TYPE_ENUM: base = t.class_name ? t.class_name : "enum"; break;
        case TYPE_NAMESPACE: base = t.class_name ? t.class_name : "namespace"; break;
        default: base = "unknown"; break;
    }

    if (t.is_tainted) *pos += snprintf(buf + *pos, max_len - *pos, "tainted ");
    else if (t.is_pristine) *pos += snprintf(buf + *pos, max_len - *pos, "pristine ");

    if (t.is_unsigned) *pos += snprintf(buf + *pos, max_len - *pos, "unsigned ");
    *pos += snprintf(buf + *pos, max_len - *pos, "%s", base);

    int ptrs = t.ptr_depth;
    if (t.array_size > 0) ptrs--;
    if (t.array_depth > 0) ptrs--;

    for(int i=0; i<ptrs; i++) {
        if(*pos < max_len - 1) buf[(*pos)++] = '*';
    }
    if (t.array_size > 0) {
        *pos += snprintf(buf + *pos, max_len - *pos, "[]");
        if (t.array_depth > 0) {
            *pos += snprintf(buf + *pos, max_len - *pos, "[]");
        }
    }
}

char* sem_type_to_str(VarType t) {
    static char buffers[16][1024];
    static int idx = 0;
    char *buf = buffers[idx];
    idx = (idx + 1) % 16;

    int pos = 0;
    sem_type_to_str_rec(t, buf, 1024, &pos);
    buf[pos] = '\0';

    return buf;
}
char* sem_mangle_type(VarType t) {
    const char *base = "unknown";
    switch(t.base) {
        case TYPE_INT: base = "i32"; break;
        case TYPE_SHORT: base = "i16"; break;
        case TYPE_LONG: base = "i64"; break;
        case TYPE_LONG_LONG: base = "i64"; break;
        case TYPE_CHAR: base = "i8"; break;
        case TYPE_BOOL: base = "bool"; break;
        case TYPE_SINGLE: base = "f32"; break;
        case TYPE_DOUBLE: base = "f64"; break;
        case TYPE_LONG_DOUBLE: base = "f64"; break;
        case TYPE_VOID: base = "void"; break;
        case TYPE_ERROR: base = "error"; break;

        case TYPE_CLASS: base = t.class_name ? t.class_name : "class"; break;
        case TYPE_ENUM: base = t.class_name ? t.class_name : "enum"; break;
        default: base = "any"; break;
    }

    static char buf[256];
    int pos = snprintf(buf, 256, "%s", base);
    for (int i = 0; i < t.ptr_depth; i++) {
        pos += snprintf(buf + pos, 256 - pos, "_p");
    }
    for (int i = 0; i < t.array_size; i++) {
        pos += snprintf(buf + pos, 256 - pos, "_arr"); // Just generic array mangling for now
    }
    return buf;
}

char* sem_mangle_itanium_type(VarType t) {
    const char *base = "v";
    switch(t.base) {
        case TYPE_INT: base = "i"; break;
        case TYPE_SHORT: base = "s"; break;
        case TYPE_LONG: base = "l"; break;
        case TYPE_LONG_LONG: base = "x"; break;
        case TYPE_CHAR: base = "c"; break;
        case TYPE_BOOL: base = "b"; break;
        case TYPE_SINGLE: base = "f"; break;
        case TYPE_DOUBLE: base = "d"; break;
        case TYPE_LONG_DOUBLE: base = "e"; break; // wait, long double is 'e'
        case TYPE_VOID: base = "v"; break;
        case TYPE_CLASS:
        case TYPE_ENUM: {
            static char cbuf[256];
            snprintf(cbuf, 256, "%zu%s", strlen(t.class_name ? t.class_name : "unknown"), t.class_name ? t.class_name : "unknown");
            base = cbuf;
            break;
        }
        default: base = "v"; break;
    }
    static char buf[256];
    int pos = 0;
    for (int i = 0; i < t.ptr_depth; i++) {
        pos += snprintf(buf + pos, 256 - pos, "P");
    }
    snprintf(buf + pos, 256 - pos, "%s", base);
    return buf;
}

char* sem_mangle_itanium_func_name(SemanticCtx *ctx, const char *class_name, const char *base_name, Parameter *params) {
    char buf[1024];
    int pos = 0;
    if (class_name) {
        pos += snprintf(buf + pos, 1024 - pos, "_ZN%zu%s%zu%sE", strlen(class_name), class_name, strlen(base_name), base_name);
    } else {
        pos += snprintf(buf + pos, 1024 - pos, "_Z%zu%s", strlen(base_name), base_name);
    }
    Parameter *p = params;
    while (p) {
        pos += snprintf(buf + pos, 1024 - pos, "%s", sem_mangle_itanium_type(p->type));
        p = p->next;
    }
    if (!params) pos += snprintf(buf + pos, 1024 - pos, "v");
    return ctx && ctx->compiler_ctx && ctx->compiler_ctx->arena ? arena_strdup(ctx->compiler_ctx->arena, buf) : strdup(buf);
}

char* sem_mangle_func_name(SemanticCtx *ctx, const char *class_name, const char *base_name, Parameter *params) {
    char buf[1024];
    int pos = 0;

    if (class_name) {
        pos += snprintf(buf + pos, 1024 - pos, "%s_%s", class_name, base_name);
    } else {
        pos += snprintf(buf + pos, 1024 - pos, "%s", base_name);
    }

    Parameter *p = params;
    while (p) {
        pos += snprintf(buf + pos, 1024 - pos, "_%s", sem_mangle_type(p->type));
        p = p->next;
    }

    if (ctx && ctx->compiler_ctx && ctx->compiler_ctx->arena) {
        return arena_strdup(ctx->compiler_ctx->arena, buf);
    }
    return strdup(buf);
}
