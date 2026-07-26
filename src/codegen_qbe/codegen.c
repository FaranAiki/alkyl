#include "codegen/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char qbe_type(VarType t) {
    if (t.ptr_depth > 0) return 'l';
    switch (t.base) {
        case TYPE_VOID: return 'v';
        case TYPE_INT:
        case TYPE_UNSIGNED_INT:
        case TYPE_SHORT:
        case TYPE_CHAR:
        case TYPE_UNSIGNED_CHAR:
        case TYPE_BOOL:
            return 'w';
        case TYPE_LONG:
        case TYPE_LONG_LONG:
        case TYPE_UNSIGNED_LONG:
        case TYPE_UNSIGNED_LONG_LONG:
            return 'l';
        case TYPE_SINGLE: return 's';
        case TYPE_DOUBLE: return 'd';
        default: return 'l';
    }
}

static int qbe_type_size(char qtype) {
    switch (qtype) {
        case 'w': return 4;
        case 'l': return 8;
        case 's': return 4;
        case 'd': return 8;
        default: return 8;
    }
}

static void print_val(FILE *out, AlirValue *v) {
    if (!v) return;
    switch (v->kind) {
        case ALIR_VAL_INT:
        case ALIR_VAL_CONST:
            fprintf(out, "%ld", v->val.long_val);
            break;
        case ALIR_VAL_TEMP:
            fprintf(out, "%%t%d", v->temp_id);
            break;
        case ALIR_VAL_VAR:
            if (v->val.str_val) {
                if (v->val.str_val[0] == 'p' && isdigit((unsigned char)v->val.str_val[1])) {
                    fprintf(out, "%%%s", v->val.str_val);
                } else {
                    fprintf(out, "$%s", v->val.str_val);
                }
            }
            break;
        case ALIR_VAL_GLOBAL:
            if (v->val.str_val)
                fprintf(out, "$%s", v->val.str_val);
            break;
        case ALIR_VAL_LABEL:
            if (v->val.str_val)
                fprintf(out, "@%s", v->val.str_val);
            break;
        default:
            fprintf(out, "0");
            break;
    }
}

static void emit_inst(FILE *out, AlirInst *inst, AlirBlock *next_block) {
    if (!inst) return;

    char dt = 'w';
    if (inst->dest) {
        dt = qbe_type(inst->dest->type);
        if (dt == 'v') dt = 'w';
    }

    switch (inst->op) {
        case ALIR_OP_ALLOCA: {
            int sz = qbe_type_size(dt);
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =l alloc4 %d\n", sz);
            break;
        }
        case ALIR_OP_STORE: {
            char st = qbe_type(inst->op1->type);
            if (st == 'v') st = 'w';
            fprintf(out, "\tstore%c ", st);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_LOAD: {
            char lt = qbe_type(inst->dest->type);
            if (lt == 'v') lt = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c load%c ", lt, lt);
            print_val(out, inst->op1);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_GET_PTR: {
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =l add ");
            print_val(out, inst->op1);
            if (inst->op2) {
                int idx = (int)(intptr_t)inst->op2->val.long_val;
                VarType elem_type = inst->dest->type;
                elem_type.ptr_depth--;
                int elem_size = qbe_type_size(qbe_type(elem_type));
                fprintf(out, ", %d", idx * elem_size);
            }
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_ADD:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c add ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_SUB:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c sub ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_MUL:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c mul ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_DIV:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c div ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_RET:
            if (inst->op1) {
                fprintf(out, "\tret ");
                print_val(out, inst->op1);
                fprintf(out, "\n");
            } else {
                fprintf(out, "\tret\n");
            }
            break;
        case ALIR_OP_JUMP:
            fprintf(out, "\tjmp ");
            print_val(out, inst->op1);
            fprintf(out, "\n");
            break;
        case ALIR_OP_CONDI: {
            fprintf(out, "\tjnz ");
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            if (inst->args && inst->arg_count > 0 && inst->args[0]) {
                if (inst->args[0]->kind == ALIR_VAL_LABEL) {
                    fprintf(out, ", @%s", inst->args[0]->val.str_val);
                } else {
                    print_val(out, inst->args[0]);
                }
            } else if (next_block && next_block->label) {
                fprintf(out, ", @%s", next_block->label);
            } else {
                fprintf(out, ", @L");
            }
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_CALL: {
            if (inst->dest) {
                fprintf(out, "\t");
                print_val(out, inst->dest);
                fprintf(out, " =%c ", dt);
            } else {
                fprintf(out, "\t");
            }
            fprintf(out, "call ");
            print_val(out, inst->op1);
            fprintf(out, "(");
            for (int i = 0; i < inst->arg_count; i++) {
                char at = qbe_type(inst->args[i]->type);
                if (at == 'v') at = 'w';
                fprintf(out, "%c ", at);
                print_val(out, inst->args[i]);
                if (i < inst->arg_count - 1) fprintf(out, ", ");
            }
            fprintf(out, ")\n");
            break;
        }
        case ALIR_OP_LT:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w csltw ");
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_GT:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w csgtw ");
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_EQ:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w ceqw ");
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_CAST: {
            fprintf(out, "\t");
            print_val(out, inst->dest);
            char src_t = 'w';
            if (inst->op1) {
                src_t = qbe_type(inst->op1->type);
                if (src_t == 'v') src_t = 'w';
            }
            if (dt == 'l' && src_t == 'w') {
                fprintf(out, " =l extsw ");
            } else if (dt == 'w' && src_t == 'l') {
                fprintf(out, " =w copy ");
            } else {
                fprintf(out, " =%c copy ", dt);
            }
            print_val(out, inst->op1);
            fprintf(out, "\n");
            break;
        }
        default:
            fprintf(out, "\t# UNHANDLED OP %d\n", inst->op);
            break;
    }
}

int backend_run(AlirModule *module, const char *basename, const char *link_flags) {
    char outname[256];
    snprintf(outname, sizeof(outname), "%s.ssa", basename);
    FILE *out = fopen(outname, "w");
    if (!out) {
        perror("fopen");
        return 1;
    }

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
            fprintf(out, "export function $%s(", f->name);
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

        AlirBlock *curr_block = f->blocks;
        while (curr_block) {
            AlirBlock *next_block = curr_block->next;
            fprintf(out, "\t@%s\n", curr_block->label ? curr_block->label : "L");
            for (AlirInst *i = curr_block->head; i; i = i->next) {
                emit_inst(out, i, next_block);
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

    snprintf(cmd, sizeof(cmd), "gcc %s.s %s -o %s", basename, link_flags ? link_flags : "", basename);
    int gcc_ret = system(cmd);
    if (gcc_ret != 0) {
        fprintf(stderr, "GCC linking failed with code %d\n", gcc_ret);
        return 1;
    }

    return 0;
}
