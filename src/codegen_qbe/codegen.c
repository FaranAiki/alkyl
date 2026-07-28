#include "codegen/codegen.h"
#include "codegen_qbe/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int s_next_qbe_temp = 0;

static int get_qbe_type_size_for_var(VarType t) {
    if (t.ptr_depth > 0) return 8;
    if (t.array_size > 0) return 8;
    switch (t.base) {
        case TYPE_VOID: return 0;
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_UNSIGNED_CHAR: return 1;
        case TYPE_SHORT: return 2;
        case TYPE_INT:
        case TYPE_UNSIGNED_INT:
        case TYPE_SINGLE: return 4;
        case TYPE_LONG:
        case TYPE_LONG_LONG:
        case TYPE_UNSIGNED_LONG:
        case TYPE_UNSIGNED_LONG_LONG:
        case TYPE_DOUBLE:
        case TYPE_LONG_DOUBLE: return 8;
        default: return 8;
    }
}

static int get_qbe_type_align_for_var(VarType t) {
    if (t.ptr_depth > 0) return 8;
    if (t.array_size > 0) return 8;
    switch (t.base) {
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_UNSIGNED_CHAR: return 1;
        case TYPE_SHORT: return 2;
        case TYPE_INT:
        case TYPE_UNSIGNED_INT:
        case TYPE_SINGLE: return 4;
        case TYPE_LONG:
        case TYPE_LONG_LONG:
        case TYPE_UNSIGNED_LONG:
        case TYPE_UNSIGNED_LONG_LONG:
        case TYPE_DOUBLE:
        case TYPE_LONG_DOUBLE: return 8;
        default: return 8;
    }
}

static int get_struct_field_offset(AlirModule *module, const char *struct_name, int field_index) {
    AlirStruct *st = alir_find_struct(module, struct_name);
    if (!st || !st->fields) return field_index * 8;
    
    int offset = 0;
    AlirField *f = st->fields;
    while (f) {
        int size = get_qbe_type_size_for_var(f->type);
        int align = get_qbe_type_align_for_var(f->type);
        offset = (offset + align - 1) & ~(align - 1);
        if (f->index == field_index) {
            return offset;
        }
        offset += size;
        f = f->next;
    }
    return field_index * 8;
}

static int get_struct_size(AlirModule *module, const char *struct_name) {
    AlirStruct *st = alir_find_struct(module, struct_name);
    if (!st || !st->fields) return 8;
    
    int offset = 0;
    AlirField *f = st->fields;
    while (f) {
        int size = get_qbe_type_size_for_var(f->type);
        int align = get_qbe_type_align_for_var(f->type);
        offset = (offset + align - 1) & ~(align - 1);
        offset += size;
        f = f->next;
    }
    return (offset + 7) & ~7;
}

int find_max_temp(AlirModule *module) {
    int max_temp = 0;
    for (AlirFunction *f = module->functions; f; f = f->next) {
        for (AlirBlock *b = f->blocks; b; b = b->next) {
            for (AlirInst *i = b->head; i; i = i->next) {
                if (i->dest && i->dest->kind == ALIR_VAL_TEMP && i->dest->temp_id >= max_temp) {
                    max_temp = i->dest->temp_id + 1;
                }
            }
        }
    }
    return max_temp;
}

int alloc_qbe_temp(void) {
    return s_next_qbe_temp++;
}

void emit_inst(FILE *out, AlirModule *module, AlirInst *inst, AlirBlock *next_block) {
    if (!inst) return;

    char dt = 'w';
    if (inst->dest) {
        dt = qbe_type(inst->dest->type);
        if (dt == 'v') dt = 'w';
    }

    switch (inst->op) {
        case ALIR_OP_ALLOCA: {
            int sz;
            if (inst->op1 && inst->op1->kind == ALIR_VAL_CONST) {
                sz = (int)inst->op1->val.long_val;
            } else if (inst->op1 && inst->op1->kind == ALIR_VAL_INT) {
                sz = (int)inst->op1->val.long_val;
            } else if (inst->dest && inst->dest->type.base == TYPE_CLASS && inst->dest->type.class_name) {
                sz = get_struct_size(module, inst->dest->type.class_name);
            } else {
                sz = qbe_type_size(dt);
            }
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
            
            int offset = 0;
            if (inst->op2) {
                VarType base_type = inst->op1->type;
                if (base_type.ptr_depth > 0) base_type.ptr_depth--;
                else if (base_type.array_size > 0) base_type.array_size = 0;
                
                if (base_type.base == TYPE_CLASS && base_type.class_name && inst->op2->kind == ALIR_VAL_CONST) {
                    offset = get_struct_field_offset(module, base_type.class_name, (int)inst->op2->val.long_val);
                } else {
                    int idx = (int)(intptr_t)inst->op2->val.long_val;
                    VarType elem_type = inst->dest->type;
                    elem_type.ptr_depth--;
                    int elem_size = qbe_type_size(qbe_type(elem_type));
                    offset = idx * elem_size;
                }
            }
            fprintf(out, ", %d", offset);
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
        case ALIR_OP_SHL:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c shl ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_SHR: {
            int is_unsigned = inst->op1 && inst->op1->type.is_unsigned;
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c %s ", dt, is_unsigned ? "shr" : "sar");
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
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
            char call_dt = dt;
            if (inst->op1 && inst->op1->kind == ALIR_VAL_VAR && inst->op1->val.str_val) {
                for (AlirFunction *f = module->functions; f; f = f->next) {
                    if (f->name && strcmp(f->name, inst->op1->val.str_val) == 0) {
                        char rt = qbe_type(f->ret_type);
                        if (rt != 'v') call_dt = rt;
                        break;
                    }
                }
            }
            if (inst->dest) {
                fprintf(out, "\t");
                print_val(out, inst->dest);
                fprintf(out, " =%c ", call_dt);
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
        case ALIR_OP_BITCAST: {
            char src_t = 'l';
            if (inst->op1) {
                src_t = qbe_type(inst->op1->type);
                if (src_t == 'v') src_t = 'l';
            }
            if (dt == 'l' && src_t == 'l') {
                fprintf(out, "\t");
                print_val(out, inst->dest);
                fprintf(out, " =l copy ");
                print_val(out, inst->op1);
                fprintf(out, "\n");
            } else {
                fprintf(out, "\t");
                print_val(out, inst->dest);
                fprintf(out, " =%c copy ", dt);
                print_val(out, inst->op1);
                fprintf(out, "\n");
            }
            break;
        }
        case ALIR_OP_FREE_STACK:
            break;
        case ALIR_OP_MOD:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c mod ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_FADD:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c add ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_FSUB:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c sub ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_FMUL:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c mul ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_FDIV:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c div ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_AND:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c and ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_OR:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c or ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_XOR:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c xor ", dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        case ALIR_OP_NOT:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w ceqw ");
            print_val(out, inst->op1);
            fprintf(out, ", 0\n");
            break;
        case ALIR_OP_ROTR: {
            int t_bits_minus_y = alloc_qbe_temp();
            int t_shr = alloc_qbe_temp();
            int t_shl = alloc_qbe_temp();

            fprintf(out, "\t%%t%d =%c sub ", t_bits_minus_y, dt);
            print_val(out, inst->op2);
            fprintf(out, "\n");

            fprintf(out, "\t%%t%d =%c shr ", t_shr, dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");

            fprintf(out, "\t%%t%d =%c shl ", t_shl, dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            fprintf(out, "%%t%d", t_bits_minus_y);
            fprintf(out, "\n");

            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c or ", dt);
            fprintf(out, "%%t%d, %%t%d", t_shr, t_shl);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_ROTL: {
            int t_shl = alloc_qbe_temp();
            int t_bits_minus_y = alloc_qbe_temp();
            int t_shr = alloc_qbe_temp();

            fprintf(out, "\t%%t%d =%c shl ", t_shl, dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");

            fprintf(out, "\t%%t%d =%c sub ", t_bits_minus_y, dt);
            print_val(out, inst->op2);
            fprintf(out, "\n");

            fprintf(out, "\t%%t%d =%c shr ", t_shr, dt);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            fprintf(out, "%%t%d", t_bits_minus_y);
            fprintf(out, "\n");

            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c or ", dt);
            fprintf(out, "%%t%d, %%t%d", t_shl, t_shr);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_LTE:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w csltw ");
            print_val(out, inst->op2);
            fprintf(out, ", ");
            print_val(out, inst->op1);
            fprintf(out, "\n");
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w not ");
            print_val(out, inst->dest);
            fprintf(out, "\n");
            break;
        case ALIR_OP_GTE:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w csltw ");
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w not ");
            print_val(out, inst->dest);
            fprintf(out, "\n");
            break;
        case ALIR_OP_NEQ:
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w ceqw ");
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w not ");
            print_val(out, inst->dest);
            fprintf(out, "\n");
            break;
        case ALIR_OP_PANIC:
            if (inst->op1) {
                fprintf(out, "\tcall $printf(l ");
                print_val(out, inst->op1);
                fprintf(out, ", w ");
                print_val(out, inst->op2);
                fprintf(out, ")\n");
            }
            fprintf(out, "\tcall $exit(l 1)\n");
            break;
        case ALIR_OP_FALLBACK:
            fprintf(out, "\t# FALLBACK (ternary/null-coalescing) unhandled\n");
            break;
        default:
            fprintf(out, "\t# UNHANDLED OP %d\n", inst->op);
            break;
    }
}

