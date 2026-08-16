/**
 * @file driver.c
 * @brief QBE code generation driver implementation.
 */
#include "codegen/codegen.h"
#include "codegen_qbe/codegen.h"
#include "common/linker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int backend_run_alir(AlirModule *module, const char *basename, const char *link_flags, int optimization_level, LinkerType linker) {
    (void)optimization_level;
    char outname[256];
    snprintf(outname, sizeof(outname), "%s.ssa", basename);
    FILE *out = fopen(outname, "w");
    if (!out) {
        perror("fopen");
        return 1;
    }

    s_next_qbe_temp = find_max_temp(module);

    for (AlirGlobal *g = module->globals; g; g = g->next) {
        if (g->string_content) {
            fprintf(out, "data $%s = { b \"", g->name);
            for (int i = 0; g->string_content[i]; i++) {
                char c = g->string_content[i];
                if (c == '\n') fprintf(out, "\\n");
                else if (c == '\r') fprintf(out, "\\r");
                else if (c == '\t') fprintf(out, "\\t");
                else if (c == '\"') fprintf(out, "\\\"");
                else if (c == '\\') fprintf(out, "\\\\");
                else fprintf(out, "%c", c);
            }
            fprintf(out, "\", b 0 }\n");
        } else {
            fprintf(out, "data $%s = { z 8 }\n", g->name);
        }
    }

    for (AlirFunction *f = module->functions; f; f = f->next) {
        if (!f->blocks) continue;
        char ret_t = qbe_type(f->ret_type);
        if (ret_t == 'v') {
            fprintf(out, "export function l $%s(", f->name);
        } else {
            fprintf(out, "export function %c $%s(", ret_t, f->name);
        }

        int p_idx = 0;
        AlirParam *p = f->params;
        while (p) {
            char pt = qbe_type(p->type);
            if (pt == 'v') pt = 'w';
            fprintf(out, "%c %%p%d", pt, p_idx++);
            p = p->next;
            if (p) fprintf(out, ", ");
        }
        fprintf(out, ") {\n");

        s_current_qbe_function = f;
        AlirBlock *curr_block = f->blocks;
        while (curr_block) {
            AlirBlock *next_block = curr_block->next;
            fprintf(out, "\t@%s\n", curr_block->label ? curr_block->label : "L");
            for (AlirInst *i = curr_block->head; i; i = i->next) {
                emit_inst(out, module, i, next_block);
            }
            curr_block = next_block;
        }
        fprintf(out, "}\n");
    }

    fclose(out);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "qbe %s.ssa -o %s.s", basename, basename);
    int qbe_ret = system(cmd);
    if (qbe_ret != 0) {
        fprintf(stderr, "QBE failed with code %d\n", qbe_ret);
        return 1;
    }

    char s_file[256];
    snprintf(s_file, sizeof(s_file), "%s.s", basename);
    int link_ret = alkyl_link(s_file, basename, link_flags, linker);
    if (link_ret != 0) {
        fprintf(stderr, "Linking failed.\n");
        return 1;
    }

    return 0;
}
