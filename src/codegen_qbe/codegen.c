#include "codegen/codegen.h"
#include "codegen_qbe/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int s_next_qbe_temp = 0;
AlirFunction *s_current_qbe_function = NULL;

static int get_qbe_type_size_for_var(VarType t) {
    if (t.ptr_depth > 0) return 8;
    if (t.is_tainted) return 8;
    if (t.array_size > 0) {
        VarType elem = t;
        elem.array_size = 0;
        return t.array_size * get_qbe_type_size_for_var(elem);
    }
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
    if (t.is_tainted) return 4;
    if (t.array_size > 0) {
        VarType elem = t;
        elem.array_size = 0;
        return get_qbe_type_align_for_var(elem);
    }
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
        if (dt == 'v') dt = 'l';
    }

    switch (inst->op) {
        case ALIR_OP_ALLOCA: {
            int sz;
            int align = get_qbe_type_align_for_var(inst->dest->type);
            if (inst->op1 && inst->op1->kind == ALIR_VAL_CONST) {
                sz = (int)inst->op1->val.long_val;
            } else if (inst->op1 && inst->op1->kind == ALIR_VAL_INT) {
                sz = (int)inst->op1->val.long_val;
            } else if (inst->dest && inst->dest->type.base == TYPE_CLASS && inst->dest->type.class_name) {
                sz = get_struct_size(module, inst->dest->type.class_name);
            } else {
                sz = get_qbe_type_size_for_var(inst->dest->type);
            }
            if (inst->dest && inst->dest->type.base == TYPE_CLASS && inst->dest->type.class_name && strncmp(inst->dest->type.class_name, "FluxCtx", 7) == 0) {
                fprintf(out, "\t");
                print_val(out, inst->dest);
                fprintf(out, " =l call $malloc(l %d)\n", sz);
            } else {
                fprintf(out, "\t");
                print_val(out, inst->dest);
                fprintf(out, " =l alloc%d %d\n", align > 4 ? align : 4, sz);
            }
            break;
        }
        case ALIR_OP_STORE: {
            char st = qbe_type(inst->op1->type);
            if (inst->op1->type.base == TYPE_CLASS && inst->op1->type.ptr_depth == 0) {
                int sz = 4;
                if (inst->op1->type.base == TYPE_CLASS && inst->op1->type.class_name) {
                    sz = get_struct_size(module, inst->op1->type.class_name);
                } else {
                    sz = get_qbe_type_size_for_var(inst->op1->type);
                }
                fprintf(out, "\tblit ");
                print_val(out, inst->op1);
                fprintf(out, ", ");
                print_val(out, inst->op2);
                fprintf(out, ", %d\n", sz);
                break;
            }
            fprintf(out, "\tstore%c ", st);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_LOAD: {
            char lt = qbe_type(inst->dest->type);
            if (inst->dest->type.base == TYPE_CLASS && inst->dest->type.ptr_depth == 0) {
                fprintf(out, "\t");
                print_val(out, inst->dest);
                fprintf(out, " =l copy ");
                print_val(out, inst->op1);
                fprintf(out, "\n");
                break;
            }
            if (lt == 'v') lt = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c load%c ", lt, lt);
            print_val(out, inst->op1);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_GET_PTR: {
            if (!inst->op2) {
                fprintf(out, "\t");
                print_val(out, inst->dest);
                fprintf(out, " =l add ");
                print_val(out, inst->op1);
                fprintf(out, ", 0\n");
                break;
            }

            if (inst->op2->kind != ALIR_VAL_CONST) {
                char idx_t = qbe_type(inst->op2->type);
                if (idx_t == 'v') idx_t = 'w';
                
                VarType elem_type = inst->dest->type;
                elem_type.ptr_depth--;
                int elem_size = get_qbe_type_size_for_var(elem_type);

                int tmp_id = 0;
                if (inst->dest->kind == ALIR_VAL_TEMP) tmp_id = inst->dest->temp_id;

                // 1. Extend index to 64-bit if necessary
                if (idx_t == 'w') {
                    fprintf(out, "\t%%__idx_ext_%d =l extsw ", tmp_id);
                    print_val(out, inst->op2);
                    fprintf(out, "\n");
                } else if (idx_t == 'b') {
                    fprintf(out, "\t%%__idx_ext_%d =l extub ", tmp_id);
                    print_val(out, inst->op2);
                    fprintf(out, "\n");
                }

                // 2. Multiply by element size
                if (elem_size > 1) {
                    fprintf(out, "\t%%__idx_mul_%d =l mul ", tmp_id);
                    if (idx_t == 'w' || idx_t == 'b') fprintf(out, "%%__idx_ext_%d", tmp_id);
                    else print_val(out, inst->op2);
                    fprintf(out, ", %d\n", elem_size);
                }

                // 3. Add to base pointer
                fprintf(out, "\t");
                print_val(out, inst->dest);
                fprintf(out, " =l add ");
                print_val(out, inst->op1);
                fprintf(out, ", ");
                if (elem_size > 1) fprintf(out, "%%__idx_mul_%d", tmp_id);
                else {
                    if (idx_t == 'w' || idx_t == 'b') fprintf(out, "%%__idx_ext_%d", tmp_id);
                    else print_val(out, inst->op2);
                }
                fprintf(out, "\n");
                break;
            }

            // Constant offset
            VarType base_type = inst->op1->type;
            if (base_type.ptr_depth > 0) base_type.ptr_depth--;
            else if (base_type.array_size > 0) base_type.array_size = 0;
            
            int offset = 0;
            if (base_type.base == TYPE_CLASS && base_type.class_name) {
                offset = get_struct_field_offset(module, base_type.class_name, (int)inst->op2->val.long_val);
            } else {
                int idx = (int)inst->op2->val.long_val;
                VarType elem_type = inst->dest->type;
                elem_type.ptr_depth--;
                int elem_size = get_qbe_type_size_for_var(elem_type);
                offset = idx * elem_size;
            }

            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =l add ");
            print_val(out, inst->op1);
            fprintf(out, ", %d\n", offset);
            break;
        }
        case ALIR_OP_ADD: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c add ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_SUB: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c sub ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_MUL: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c mul ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_DIV: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c div ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_SHL: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c shl ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_SHR: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            int is_unsigned = inst->op1 && inst->op1->type.is_unsigned;
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c %s ", op_t, is_unsigned ? "shr" : "sar");
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_RET:
            if (inst->op1) {
                if (s_current_qbe_function && s_current_qbe_function->ret_type.is_tainted) {
                    char rt = qbe_type(inst->op1->type);
                    if (rt == 'v') rt = 'w'; // Just in case
                    if (rt == 'w') {
                        int ext_id = s_next_qbe_temp++;
                        int shl_id = s_next_qbe_temp++;
                        fprintf(out, "\t%%ret_ext_%d =l extuw ", ext_id);
                        print_val(out, inst->op1);
                        fprintf(out, "\n\t%%ret_shl_%d =l shl %%ret_ext_%d, 32\n", shl_id, ext_id);
                        fprintf(out, "\tret %%ret_shl_%d\n", shl_id);
                    } else {
                        fprintf(out, "\tret ");
                        print_val(out, inst->op1);
                        fprintf(out, "\n");
                    }
                } else {
                    fprintf(out, "\tret ");
                    print_val(out, inst->op1);
                    fprintf(out, "\n");
                }
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
                    if (f->name && streq(f->name, inst->op1->val.str_val)) {
                        char rt = qbe_type(f->ret_type);
                        if (f->ret_type.base == TYPE_CLASS && f->ret_type.ptr_depth == 0) call_dt = 'l';
                        else if (rt != 'v') call_dt = rt;
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
        case ALIR_OP_GT:
        case ALIR_OP_EQ: {
            char t1 = 'w';
            int is_unsigned = 0;
            if (inst->op1) {
                t1 = qbe_type(inst->op1->type);
                if (t1 == 'v') t1 = 'w';
                is_unsigned = inst->op1->type.is_unsigned;
            }
            const char *op_str = "";
            switch (inst->op) {
                case ALIR_OP_EQ:
                    if (t1 == 's' || t1 == 'd') op_str = (t1 == 's') ? "ceqs" : "ceqd";
                    else op_str = (t1 == 'l') ? "ceql" : "ceqw";
                    break;
                case ALIR_OP_LT:
                    if (t1 == 's' || t1 == 'd') op_str = (t1 == 's') ? "clts" : "cltd";
                    else if (is_unsigned) op_str = (t1 == 'l') ? "cultl" : "cultw";
                    else op_str = (t1 == 'l') ? "csltl" : "csltw";
                    break;
                case ALIR_OP_GT:
                    if (t1 == 's' || t1 == 'd') op_str = (t1 == 's') ? "cgts" : "cgtd";
                    else if (is_unsigned) op_str = (t1 == 'l') ? "cugtl" : "cugtw";
                    else op_str = (t1 == 'l') ? "csgtl" : "csgtw";
                    break;
                default: break;
            }
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w %s ", op_str);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
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
        case ALIR_OP_SIZEOF: {
            int sz = 8;
            if (inst->op1) {
                if (inst->op1->type.base == TYPE_CLASS && inst->op1->type.class_name && inst->op1->type.ptr_depth == 0) {
                    sz = get_struct_size(module, inst->op1->type.class_name);
                } else {
                    sz = get_qbe_type_size_for_var(inst->op1->type);
                }
            }
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c copy %d\n", dt, sz);
            break;
        }
        case ALIR_OP_ALIGNOF: {
            int align = 8;
            if (inst->op1) align = get_qbe_type_align_for_var(inst->op1->type);
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c copy %d\n", dt, align);
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
        case ALIR_OP_MOD: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c rem ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_FADD: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c add ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_FSUB: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c sub ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_FMUL: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c mul ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_FDIV: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c div ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_AND: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c and ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_OR: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c or ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_XOR: {
            char op_t = (inst->op1) ? qbe_type(inst->op1->type) : dt;
            if (op_t == 'v') op_t = 'w';
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c xor ", op_t);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_NOT: {
            char t1 = 'w';
            if (inst->op1) {
                t1 = qbe_type(inst->op1->type);
                if (t1 == 'v') t1 = 'w';
            }
            const char *op_str = "ceqw";
            if (t1 == 'l') op_str = "ceql";
            else if (t1 == 's') op_str = "ceqs";
            else if (t1 == 'd') op_str = "ceqd";
            fprintf(out, "\t");
            print_val(out, inst->dest);
            if (t1 == 's') {
                fprintf(out, " =w %s ", op_str);
                print_val(out, inst->op1);
                fprintf(out, ", s_0.0\n"); /* Single precision 0.0 */
            } else if (t1 == 'd') {
                fprintf(out, " =w %s ", op_str);
                print_val(out, inst->op1);
                fprintf(out, ", d_0.0\n"); /* Double precision 0.0 */
            } else {
                fprintf(out, " =w %s ", op_str);
                print_val(out, inst->op1);
                fprintf(out, ", 0\n");
            }
            break;
        }
        case ALIR_OP_ROTR: {
            int t_bits_minus_y = alloc_qbe_temp();
            int t_shr = alloc_qbe_temp();
            int t_shl = alloc_qbe_temp();

            int bits = (dt == 'l') ? 64 : 32;
            fprintf(out, "\t%%t%d =%c sub %d, ", t_bits_minus_y, dt, bits);
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

            int bits = (dt == 'l') ? 64 : 32;
            fprintf(out, "\t%%t%d =%c sub %d, ", t_bits_minus_y, dt, bits);
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
        case ALIR_OP_GTE:
        case ALIR_OP_NEQ: {
            char t1 = 'w';
            int is_unsigned = 0;
            if (inst->op1) {
                t1 = qbe_type(inst->op1->type);
                if (t1 == 'v') t1 = 'w';
                is_unsigned = inst->op1->type.is_unsigned;
            }
            const char *op_str = "";
            switch (inst->op) {
                case ALIR_OP_NEQ:
                    if (t1 == 's' || t1 == 'd') op_str = (t1 == 's') ? "cnes" : "cned";
                    else op_str = (t1 == 'l') ? "cnel" : "cnew";
                    break;
                case ALIR_OP_LTE:
                    if (t1 == 's' || t1 == 'd') op_str = (t1 == 's') ? "cles" : "cled";
                    else if (is_unsigned) op_str = (t1 == 'l') ? "culel" : "culew";
                    else op_str = (t1 == 'l') ? "cslel" : "cslew";
                    break;
                case ALIR_OP_GTE:
                    if (t1 == 's' || t1 == 'd') op_str = (t1 == 's') ? "cges" : "cged";
                    else if (is_unsigned) op_str = (t1 == 'l') ? "cugel" : "cugew";
                    else op_str = (t1 == 'l') ? "csgel" : "csgew";
                    break;
                default: break;
            }
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =w %s ", op_str);
            print_val(out, inst->op1);
            fprintf(out, ", ");
            print_val(out, inst->op2);
            fprintf(out, "\n");
            break;
        }
        case ALIR_OP_PANIC:
            if (s_current_qbe_function && s_current_qbe_function->ret_type.is_tainted) {
                if (inst->op2) {
                    fprintf(out, "\t%%err_ext_%d =l extuw ", s_next_qbe_temp++);
                    print_val(out, inst->op2);
                    fprintf(out, "\n\tret %%err_ext_%d\n", s_next_qbe_temp - 1);
                } else {
                    fprintf(out, "\tret 1\n");
                }
            } else {
                if (inst->op1) {
                    fprintf(out, "\tcall $printf(l ");
                    print_val(out, inst->op1);
                    fprintf(out, ", w ");
                    print_val(out, inst->op2 ? inst->op2 : inst->op1); // fallback
                    fprintf(out, ")\n");
                }
                fprintf(out, "\tcall $exit(l 1)\n");
                fprintf(out, "\thlt\n");
            }
            break;
        case ALIR_OP_FALLBACK: {
            int lbl = s_next_qbe_temp++;
            char rt = qbe_type(inst->dest->type);
            if (rt == 'v') rt = 'w';
            char op1_t = qbe_type(inst->op1->type);
            char op2_t = qbe_type(inst->op2->type);
            
            fprintf(out, "\t%%err_%d =w copy ", lbl);
            print_val(out, inst->op1);
            fprintf(out, "\n");
            
            if (inst->arg_count == 1 && inst->args[0]) {
                fprintf(out, "\t%%is_err_%d =w ceqw %%err_%d, ", lbl, lbl);
                print_val(out, inst->args[0]);
                fprintf(out, "\n");
            } else {
                fprintf(out, "\t%%is_err_%d =w cnew %%err_%d, 0\n", lbl, lbl);
            }
            
            fprintf(out, "\tjnz %%is_err_%d, @fb_then_%d, @fb_else_%d\n", lbl, lbl, lbl);
            
            fprintf(out, "@fb_then_%d\n", lbl);
            if (rt == 'l' && op2_t == 'w') {
                fprintf(out, "\t%%fb_val_%d =l extuw ", lbl);
                print_val(out, inst->op2);
                fprintf(out, "\n\t%%fb_then_res_%d =l shl %%fb_val_%d, 32\n", lbl, lbl);
            } else {
                fprintf(out, "\t%%fb_then_res_%d =%c copy ", lbl, rt);
                print_val(out, inst->op2);
                fprintf(out, "\n");
            }
            fprintf(out, "\tjmp @fb_merge_%d\n", lbl);
            
            fprintf(out, "@fb_else_%d\n", lbl);
            if (rt == 'w' && op1_t == 'l') {
                fprintf(out, "\t%%fb_shr_%d =l shr ", lbl);
                print_val(out, inst->op1);
                fprintf(out, ", 32\n");
                fprintf(out, "\t%%fb_else_res_%d =w copy %%fb_shr_%d\n", lbl, lbl);
            } else {
                fprintf(out, "\t%%fb_else_res_%d =%c copy ", lbl, rt);
                print_val(out, inst->op1);
                fprintf(out, "\n");
            }
            fprintf(out, "\tjmp @fb_merge_%d\n", lbl);
            
            fprintf(out, "@fb_merge_%d\n", lbl);
            fprintf(out, "\t");
            print_val(out, inst->dest);
            fprintf(out, " =%c phi @fb_then_%d %%fb_then_res_%d, @fb_else_%d %%fb_else_res_%d\n", 
                    rt, lbl, lbl, lbl, lbl);
            break;
        }
        default:
            fprintf(out, "\t# UNHANDLED OP %d\n", inst->op);
            break;
    }
}
