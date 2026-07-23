#include "codegen/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void emit_inst(FILE *out, AlirInst *inst) {
    if (!inst) return;
    
    char dt = 'w';
    if (inst->dest) {
        dt = qbe_type(inst->dest->type);
        if (dt == 'v') dt = 'w';
    }

    switch (inst->op) {
        case ALIR_OP_ALLOCA:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =l alloc4 8\n");
            break;
        case ALIR_OP_STORE:
            fprintf(out, "\tstore%c ", qbe_type(inst->op1->type) == 'v' ? 'w' : qbe_type(inst->op1->type));
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_LOAD:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c load%c ", dt, dt);
            print_val(out, inst->op1);
            fprintf(out, "\n");
            break;
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
        case ALIR_OP_RET:
            fprintf(out, "\tret");
            if (inst->op1) {
                fprintf(out, " ");
                print_val(out, inst->op1);
            }
            fprintf(out, "\n");
            break;
        case ALIR_OP_JUMP:
            fprintf(out, "\tjmp ");
            print_val(out, inst->op1);
            fprintf(out, "\n");
            break;
        case ALIR_OP_CONDI:
            fprintf(out, "\tjnz ");
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, ", ");
            print_val(out, inst->dest); // assume dest is else_label if present, though we might need to verify
            fprintf(out, "\n");
            break;
        case ALIR_OP_CALL:
            fprintf(out, "\t");
            if (inst->dest) {
                fprintf(out, "%%t%d =%c ", inst->dest->temp_id, dt);
            }
            fprintf(out, "call ");
            print_val(out, inst->op1);
            fprintf(out, "(");
            for (int i = 0; i < inst->arg_count; i++) {
                char at = qbe_type(inst->args[i]->type);
                fprintf(out, "%c ", at == 'v' ? 'w' : at);
                print_val(out, inst->args[i]);
                if (i < inst->arg_count - 1) fprintf(out, ", ");
            }
            fprintf(out, ")\n");
            break;
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
                // To truncate, QBE allows assigning with =w or using copy. 
                // Let's just use copy for now.
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

    for (AlirFunction *f = module->functions; f; f = f->next) {
        char ret_t = qbe_type(f->ret_type);
        if (ret_t == 'v') {
            fprintf(out, "export function $%s(", f->name);
        } else {
            fprintf(out, "export function %c $%s(", ret_t, f->name);
        }
        
        AlirParam *p = f->params;
        while (p) {
            fprintf(out, "%c %%%s", qbe_type(p->type) == 'v' ? 'w' : qbe_type(p->type), p->name);
            p = p->next;
            if (p) fprintf(out, ", ");
        }
        fprintf(out, ") {\n");

        for (AlirBlock *b = f->blocks; b; b = b->next) {
            fprintf(out, "@%s\n", b->label ? b->label : "L");
            for (AlirInst *i = b->head; i; i = i->next) {
                emit_inst(out, i);
            }
        }
        fprintf(out, "}\n");
    }

    fclose(out);
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "qbe %s.ssa -o %s.s", basename, basename);
    system(cmd);
    
    snprintf(cmd, sizeof(cmd), "gcc %s.s %s -o %s", basename, link_flags ? link_flags : "", basename);
    system(cmd);

    return 0;
}
