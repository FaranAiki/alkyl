#include "codegen/codegen.h"
#include "codegen_qbe/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char qbe_type(VarType t) {
    if (t.ptr_depth > 0) return 'l';
    if (t.is_tainted) return 'l';
    if (t.array_size > 0) {
        int sz = 0;
        VarType elem = t;
        elem.array_size = 0;
        sz = t.array_size * qbe_type_size(qbe_type(elem));
        if (sz <= 4) return 'w';
        return 'l';
    }
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
        case TYPE_DOUBLE:
        case TYPE_LONG_DOUBLE: return 'd';
        case TYPE_ARRAY: return 'l';
        case TYPE_AUTO:
        case TYPE_ENUM: return 'w';
        case TYPE_CLASS:
        case TYPE_NAMESPACE:
        case TYPE_ERROR:
        case TYPE_UNKNOWN: return 'l';
        default: return 'l';
    }
}

int qbe_type_size(char qtype) {
    switch (qtype) {
        case 'w': return 4;
        case 'l': return 8;
        case 's': return 4;
        case 'd': return 8;
        default: return 8;
    }
}

void print_val(FILE *out, AlirValue *v) {
    if (!v) return;
    switch (v->kind) {
        case ALIR_VAL_INT:
        case ALIR_VAL_CONST:
            fprintf(out, "%ld", v->val.long_val);
            break;
        case ALIR_VAL_SINGLE:
            fprintf(out, "%f", (double)v->val.single_val);
            break;
        case ALIR_VAL_DOUBLE:
            fprintf(out, "%lf", v->val.double_val);
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
        case ALIR_VAL_VOID:
        case ALIR_VAL_TYPE:
            fprintf(out, "0");
            break;
        default:
            fprintf(out, "0");
            break;
    }
}
