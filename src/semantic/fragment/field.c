/**
 * @file field.c
 * @brief Class field counting and collection implementation.
 */
#include "semantic.h"

int sem_count_class_fields(SemanticCtx *ctx, SemSymbol *sym) {
    if (!sym || sym->kind != SYM_CLASS) return 0;
    int count = 0;
    if (sym->parent_name) {
        SemSymbol *p = sem_symbol_lookup(ctx, sym->parent_name, NULL);
        if (p) count += sem_count_class_fields(ctx, p);
    }
    for (int i=0; i<sym->trait_count; i++) {
        SemSymbol *t = sem_symbol_lookup(ctx, sym->traits[i], NULL);
        if (t) count += sem_count_class_fields(ctx, t);
    }
    if (sym->inner_scope) {
        SemSymbol *s = sym->inner_scope->symbols;
        while(s) {
            if (s->kind == SYM_VAR) {
                count++;
            }
            s = s->next;
        }
    }
    return count;
}

int sem_count_required_class_fields(SemanticCtx *ctx, SemSymbol *sym) {
    if (!sym || sym->kind != SYM_CLASS) return 0;
    int count = 0;
    if (sym->parent_name) {
        SemSymbol *p = sem_symbol_lookup(ctx, sym->parent_name, NULL);
        if (p) count += sem_count_required_class_fields(ctx, p);
    }
    for (int i=0; i<sym->trait_count; i++) {
        SemSymbol *t = sem_symbol_lookup(ctx, sym->traits[i], NULL);
        if (t) count += sem_count_required_class_fields(ctx, t);
    }
    if (sym->inner_scope) {
        SemSymbol *s = sym->inner_scope->symbols;
        while(s) {
            if (s->kind == SYM_VAR) {
                if (s->node_ptr && s->node_ptr->type == NODE_VAR_DECL) {
                    VarDeclNode *vd = (VarDeclNode*)s->node_ptr;
                    if (!vd->initializer) {
                        count++;
                    }
                } else {
                    count++;
                }
            }
            s = s->next;
        }
    }
    return count;
}

void sem_collect_class_fields(SemanticCtx *ctx, SemSymbol *sym, VarDeclNode **fields, int *idx) {
    if (!sym || sym->kind != SYM_CLASS) return;
    if (sym->parent_name) {
        SemSymbol *p = sem_symbol_lookup(ctx, sym->parent_name, NULL);
        if (p) sem_collect_class_fields(ctx, p, fields, idx);
    }
    for (int i=0; i<sym->trait_count; i++) {
        SemSymbol *t = sem_symbol_lookup(ctx, sym->traits[i], NULL);
        if (t) sem_collect_class_fields(ctx, t, fields, idx);
    }
    if (sym->inner_scope) {
        // Collect in reverse, then reverse?
        // Wait, inner_scope->symbols are linked list prepended, so it's reversed.
        // We should count them and insert at the end.
        SemSymbol *s = sym->inner_scope->symbols;
        int local_count = 0;
        while(s) { if (s->kind == SYM_VAR) local_count++; s = s->next; }

        s = sym->inner_scope->symbols;
        int local_idx = *idx + local_count - 1;
        while(s) {
            if (s->kind == SYM_VAR) {
                if (s->node_ptr && s->node_ptr->type == NODE_VAR_DECL) {
                    fields[local_idx--] = (VarDeclNode*)s->node_ptr;
                } else {
                    fields[local_idx--] = NULL; // fallback
                }
            }
            s = s->next;
        }
        *idx += local_count;
    }
}

