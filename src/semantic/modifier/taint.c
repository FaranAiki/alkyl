#include "semantic.h"

// If `node` is a call to a function that has an attached `errnum [...]` set,
// returns that function's SemSymbol (so the caller can enumerate its errors).
// Otherwise returns NULL.
SemSymbol* sem_get_errnum_func_sym(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return NULL;
    // Unwrap assignment RHS etc. by inspecting the inner call expression.
    if (node->type == NODE_CALL) {
        CallNode *cn = (CallNode*)node;
        char *fname = cn->name;
        if (!fname && cn->target && cn->target->type == NODE_MEMBER_ACCESS) {
            fname = ((MemberAccessNode*)cn->target)->member_name;
        }
        if (fname) {
            SemSymbol *sym = sem_symbol_lookup(ctx, fname, NULL);
            if (sym && sym->kind == SYM_FUNC && sym->has_errnum) return sym;
        }
    }
    return NULL;
}

// Checks that the residue cases cover every error in `err_sym`'s error set.
// `default_case` indicates whether a catch-all default is present.
// Emits an error if uncovered (and not suppressed by a `reason`), otherwise a
// warning if the handling is partial/non-exhaustive.
void sem_check_residue_exhaustive(SemanticCtx *ctx, ASTNode *where,
                                          SemSymbol *err_sym, ResidueCase *cases,
                                          int default_case) {
    int total = err_sym->num_err;
    int covered = 0;
    for (int i = 0; i < total; i++) {
        const char *ename = err_sym->err_names[i];
        int found = default_case;
        for (ResidueCase *rc = cases; rc; rc = rc->next) {
            if (rc->is_default) { found = 1; continue; }
            for (int j = 0; j < rc->num_err; j++) {
                if (streq(rc->err_names[j], ename)) { found = 1; break; }
            }
            if (found) break;
        }
        if (found) covered++;
    }

    if (covered < total) {
        if (where && where->reason) {
            sem_hint(ctx, where, "purge may not be thorough: not all residue handled (handled %d of %d errors)",
                     covered, total);
        } else {
            sem_error(ctx, where, "not all residue is purged (handled %d of %d errors from '%s')",
                      covered, total, err_sym->name);
        }
    }
}

