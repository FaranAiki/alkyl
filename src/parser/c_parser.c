#include "c_parser.h"
#include "parser.h"
#include "typestruct.h"
#include "../common/diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void c_parser_error(CParser *p, const char *msg) {
    fprintf(stderr, "at namespace c_header:\nin %s:%d:%d: error: %s\n",
            p->lexer.filename, p->current.line, p->current.col, msg);
    p->has_error = 1;
}

static void c_eat(CParser *p, CTokenType type) {
    if (p->has_error) return;
    if (p->current.type == type) {
        p->current = c_lexer_next(&p->lexer);
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "Expected %s but found %s",
                 c_token_type_to_string(type),
                 p->current.text ? p->current.text : c_token_type_to_string(p->current.type));
        c_parser_error(p, buf);
    }
}

static int c_match(CParser *p, CTokenType type) {
    return p->current.type == type;
}

static void c_register_typedef(CParser *p, const char *name, VarType type) {
    if (!name) return;
    for (int i = 0; i < p->typedefs.count; i++) {
        if (strcmp(p->typedefs.names[i], name) == 0) {
            p->typedefs.types[i] = type;
            return;
        }
    }
    if (p->typedefs.count >= 1024) return;
    p->typedefs.names[p->typedefs.count] = arena_strdup(p->ctx->arena, name);
    p->typedefs.types[p->typedefs.count] = type;
    p->typedefs.count++;
}

static VarType c_lookup_typedef(CParser *p, const char *name) {
    for (int i = 0; i < p->typedefs.count; i++) {
        if (strcmp(p->typedefs.names[i], name) == 0) {
            return p->typedefs.types[i];
        }
    }
    if (strcmp(name, "size_t") == 0) return (VarType){TYPE_UNSIGNED_LONG, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "ptrdiff_t") == 0) return (VarType){TYPE_LONG, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "wchar_t") == 0) return (VarType){TYPE_SHORT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "char16_t") == 0 || strcmp(name, "char32_t") == 0) return (VarType){TYPE_UNSIGNED_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "int8_t") == 0) return (VarType){TYPE_CHAR, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "uint8_t") == 0) return (VarType){TYPE_UNSIGNED_CHAR, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "int16_t") == 0) return (VarType){TYPE_SHORT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "uint16_t") == 0) return (VarType){TYPE_UNSIGNED_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "int32_t") == 0) return (VarType){TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "uint32_t") == 0) return (VarType){TYPE_UNSIGNED_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "int64_t") == 0) return (VarType){TYPE_LONG_LONG, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "uint64_t") == 0) return (VarType){TYPE_UNSIGNED_LONG_LONG, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "intmax_t") == 0) return (VarType){TYPE_LONG_LONG, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "uintmax_t") == 0) return (VarType){TYPE_UNSIGNED_LONG_LONG, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "nullptr_t") == 0) return (VarType){TYPE_VOID, 1, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "max_align_t") == 0) return (VarType){TYPE_LONG_DOUBLE, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "bool") == 0 || strcmp(name, "_Bool") == 0) return (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "FILE") == 0 || strcmp(name, "__FILE") == 0) return (VarType){TYPE_CLASS, 0, arena_strdup(p->ctx->arena, "FILE"), 0, 0, NULL, NULL, 0, 0, 0, 0};
    if (strcmp(name, "va_list") == 0) return (VarType){TYPE_CHAR, 1, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    return (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
}

static Parameter* c_parse_parameters(CParser *p);

static VarType c_parse_c_type(CParser *p, int *out_ptr_depth, int *out_array_size) {
    VarType type = {TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    int ptr_depth = 0;
    int array_size = 0;
    int is_unsigned = 0;
    int is_signed = 0;
    int is_short = 0;
    int long_count = 0;

    while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT)) {
        c_eat(p, p->current.type);
    }
    while (c_match(p, C_TOKEN_IDENTIFIER) && strcmp(p->current.text, "__extension__") == 0) {
        c_eat(p, C_TOKEN_IDENTIFIER);
    }

    if (c_match(p, C_TOKEN_SIGNED)) {
        is_signed = 1;
        c_eat(p, C_TOKEN_SIGNED);
    } else if (c_match(p, C_TOKEN_UNSIGNED)) {
        is_unsigned = 1;
        c_eat(p, C_TOKEN_UNSIGNED);
    }

    if (c_match(p, C_TOKEN_SHORT)) {
        is_short = 1;
        c_eat(p, C_TOKEN_SHORT);
    }

    while (c_match(p, C_TOKEN_LONG)) {
        long_count++;
        c_eat(p, C_TOKEN_LONG);
    }

    if (c_match(p, C_TOKEN_LPAREN)) {
        c_eat(p, C_TOKEN_LPAREN);
        if (c_match(p, C_TOKEN_STAR)) {
            c_eat(p, C_TOKEN_STAR);
            type.is_func_ptr = 1;
            type.fp_ret_type = arena_alloc(p->ctx->arena, sizeof(VarType));
            *type.fp_ret_type = type;
            type.fp_param_count = 0;

            if (c_match(p, C_TOKEN_IDENTIFIER)) {
                c_eat(p, C_TOKEN_IDENTIFIER);
            }

            c_eat(p, C_TOKEN_RPAREN);

            c_eat(p, C_TOKEN_LPAREN);
            int cap = 16;
            type.fp_param_types = arena_alloc(p->ctx->arena, sizeof(VarType) * cap);
            while (!c_match(p, C_TOKEN_RPAREN) && !p->has_error) {
                VarType pt = c_parse_c_type(p, &ptr_depth, &array_size);
                (void)ptr_depth; (void)array_size;
                if (type.fp_param_count >= cap) {
                    cap *= 2;
                    VarType *new_arr = arena_alloc(p->ctx->arena, sizeof(VarType) * cap);
                    memcpy(new_arr, type.fp_param_types, sizeof(VarType) * type.fp_param_count);
                    type.fp_param_types = new_arr;
                }
                type.fp_param_types[type.fp_param_count++] = pt;
                if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);
                else if (c_match(p, C_TOKEN_ELLIPSIS)) {
                    type.fp_is_varargs = 1;
                    c_eat(p, C_TOKEN_ELLIPSIS);
                    break;
                }
                else break;
            }
            c_eat(p, C_TOKEN_RPAREN);

            type.base = TYPE_VOID;
            type.class_name = NULL;
            type.ptr_depth = 0;
            type.array_size = 0;
            if (out_ptr_depth) *out_ptr_depth = 0;
            if (out_array_size) *out_array_size = 0;
            return type;
        } else {
            int depth = 1;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) { depth++; c_eat(p, C_TOKEN_LPAREN); }
                else if (c_match(p, C_TOKEN_RPAREN)) { depth--; c_eat(p, C_TOKEN_RPAREN); }
                else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                else { c_eat(p, p->current.type); }
            }
        }
    }

    if (c_match(p, C_TOKEN_VOID)) {
        type.base = TYPE_VOID;
        c_eat(p, C_TOKEN_VOID);
    } else if (c_match(p, C_TOKEN_CHAR_KW)) {
        type.base = is_unsigned ? TYPE_UNSIGNED_CHAR : TYPE_CHAR;
        c_eat(p, C_TOKEN_CHAR_KW);
    } else if (c_match(p, C_TOKEN_SHORT) || (is_short && !is_unsigned && !is_signed)) {
        type.base = is_unsigned ? TYPE_UNSIGNED_INT : TYPE_SHORT;
        if (c_match(p, C_TOKEN_SHORT)) c_eat(p, C_TOKEN_SHORT);
        if (c_match(p, C_TOKEN_INT)) c_eat(p, C_TOKEN_INT);
    } else if (c_match(p, C_TOKEN_INT) || (is_signed && !is_short && long_count == 0)) {
        type.base = is_unsigned ? TYPE_UNSIGNED_INT : TYPE_INT;
        if (c_match(p, C_TOKEN_INT)) c_eat(p, C_TOKEN_INT);
    } else if (c_match(p, C_TOKEN_LONG) || long_count > 0) {
        if (long_count >= 2) {
            type.base = is_unsigned ? TYPE_UNSIGNED_LONG_LONG : TYPE_LONG_LONG;
        } else if (c_match(p, C_TOKEN_DOUBLE)) {
            type.base = TYPE_DOUBLE;
            c_eat(p, C_TOKEN_DOUBLE);
        } else {
            type.base = is_unsigned ? TYPE_UNSIGNED_LONG : TYPE_LONG;
        }
        while (long_count > 0 && !c_match(p, C_TOKEN_DOUBLE)) {
            long_count--;
            if (long_count == 0 && c_match(p, C_TOKEN_INT)) c_eat(p, C_TOKEN_INT);
        }
    } else if (c_match(p, C_TOKEN_FLOAT)) {
        type.base = TYPE_SINGLE;
        c_eat(p, C_TOKEN_FLOAT);
    } else if (c_match(p, C_TOKEN_DOUBLE)) {
        type.base = TYPE_DOUBLE;
        c_eat(p, C_TOKEN_DOUBLE);
    } else if (c_match(p, C_TOKEN_BOOL) || c_match(p, C_TOKEN_BOOL_KA)) {
        type.base = TYPE_BOOL;
        c_eat(p, p->current.type);
    } else if (c_match(p, C_TOKEN_STRUCT)) {
        c_eat(p, C_TOKEN_STRUCT);
        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            type.base = TYPE_CLASS;
            type.class_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);
        } else {
            type.base = TYPE_CLASS;
            type.class_name = arena_strdup(p->ctx->arena, "__anonymous_struct");
        }
    } else if (c_match(p, C_TOKEN_UNION)) {
        c_eat(p, C_TOKEN_UNION);
        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            type.base = TYPE_CLASS;
            type.class_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);
        } else {
            type.base = TYPE_CLASS;
            type.class_name = arena_strdup(p->ctx->arena, "__anonymous_union");
        }
    } else if (c_match(p, C_TOKEN_ENUM)) {
        c_eat(p, C_TOKEN_ENUM);
        type.base = TYPE_ENUM;
        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            type.class_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);
        } else {
            type.class_name = arena_strdup(p->ctx->arena, "__anonymous_enum");
        }
    } else if (c_match(p, C_TOKEN_SIZE_T)) {
        type.base = TYPE_UNSIGNED_LONG;
        c_eat(p, C_TOKEN_SIZE_T);
    } else if (c_match(p, C_TOKEN_PTRDIFF_T)) {
        type.base = TYPE_LONG;
        c_eat(p, C_TOKEN_PTRDIFF_T);
    } else if (c_match(p, C_TOKEN_WCHAR_T)) {
        type.base = TYPE_SHORT;
        c_eat(p, C_TOKEN_WCHAR_T);
    } else if (c_match(p, C_TOKEN_INT8_T)) {
        type.base = TYPE_CHAR;
        c_eat(p, C_TOKEN_INT8_T);
    } else if (c_match(p, C_TOKEN_UINT8_T)) {
        type.base = TYPE_UNSIGNED_CHAR;
        c_eat(p, C_TOKEN_UINT8_T);
    } else if (c_match(p, C_TOKEN_INT16_T)) {
        type.base = TYPE_SHORT;
        c_eat(p, C_TOKEN_INT16_T);
    } else if (c_match(p, C_TOKEN_UINT16_T)) {
        type.base = TYPE_UNSIGNED_INT;
        c_eat(p, C_TOKEN_UINT16_T);
    } else if (c_match(p, C_TOKEN_INT32_T)) {
        type.base = TYPE_INT;
        c_eat(p, C_TOKEN_INT32_T);
    } else if (c_match(p, C_TOKEN_UINT32_T)) {
        type.base = TYPE_UNSIGNED_INT;
        c_eat(p, C_TOKEN_UINT32_T);
    } else if (c_match(p, C_TOKEN_INT64_T)) {
        type.base = TYPE_LONG_LONG;
        c_eat(p, C_TOKEN_INT64_T);
    } else if (c_match(p, C_TOKEN_UINT64_T)) {
        type.base = TYPE_UNSIGNED_LONG_LONG;
        c_eat(p, C_TOKEN_UINT64_T);
    } else if (c_match(p, C_TOKEN_INT_LEAST8_T)) {
        type.base = TYPE_CHAR;
        c_eat(p, C_TOKEN_INT_LEAST8_T);
    } else if (c_match(p, C_TOKEN_UINT_LEAST8_T)) {
        type.base = TYPE_UNSIGNED_CHAR;
        c_eat(p, C_TOKEN_UINT_LEAST8_T);
    } else if (c_match(p, C_TOKEN_INT_LEAST16_T)) {
        type.base = TYPE_SHORT;
        c_eat(p, C_TOKEN_INT_LEAST16_T);
    } else if (c_match(p, C_TOKEN_UINT_LEAST16_T)) {
        type.base = TYPE_UNSIGNED_INT;
        c_eat(p, C_TOKEN_UINT_LEAST16_T);
    } else if (c_match(p, C_TOKEN_INT_LEAST32_T)) {
        type.base = TYPE_INT;
        c_eat(p, C_TOKEN_INT_LEAST32_T);
    } else if (c_match(p, C_TOKEN_UINT_LEAST32_T)) {
        type.base = TYPE_UNSIGNED_INT;
        c_eat(p, C_TOKEN_UINT_LEAST32_T);
    } else if (c_match(p, C_TOKEN_INT_LEAST64_T)) {
        type.base = TYPE_LONG_LONG;
        c_eat(p, C_TOKEN_INT_LEAST64_T);
    } else if (c_match(p, C_TOKEN_UINT_LEAST64_T)) {
        type.base = TYPE_UNSIGNED_LONG_LONG;
        c_eat(p, C_TOKEN_UINT_LEAST64_T);
    } else if (c_match(p, C_TOKEN_INT_FAST8_T)) {
        type.base = TYPE_CHAR;
        c_eat(p, C_TOKEN_INT_FAST8_T);
    } else if (c_match(p, C_TOKEN_UINT_FAST8_T)) {
        type.base = TYPE_UNSIGNED_CHAR;
        c_eat(p, C_TOKEN_UINT_FAST8_T);
    } else if (c_match(p, C_TOKEN_INT_FAST16_T)) {
        type.base = TYPE_SHORT;
        c_eat(p, C_TOKEN_INT_FAST16_T);
    } else if (c_match(p, C_TOKEN_UINT_FAST16_T)) {
        type.base = TYPE_UNSIGNED_INT;
        c_eat(p, C_TOKEN_UINT_FAST16_T);
    } else if (c_match(p, C_TOKEN_INT_FAST32_T)) {
        type.base = TYPE_INT;
        c_eat(p, C_TOKEN_INT_FAST32_T);
    } else if (c_match(p, C_TOKEN_UINT_FAST32_T)) {
        type.base = TYPE_UNSIGNED_INT;
        c_eat(p, C_TOKEN_UINT_FAST32_T);
    } else if (c_match(p, C_TOKEN_INT_FAST64_T)) {
        type.base = TYPE_LONG_LONG;
        c_eat(p, C_TOKEN_INT_FAST64_T);
    } else if (c_match(p, C_TOKEN_UINT_FAST64_T)) {
        type.base = TYPE_UNSIGNED_LONG_LONG;
        c_eat(p, C_TOKEN_UINT_FAST64_T);
    } else if (c_match(p, C_TOKEN_INTMAX_T)) {
        type.base = TYPE_LONG_LONG;
        c_eat(p, C_TOKEN_INTMAX_T);
    } else if (c_match(p, C_TOKEN_UINTMAX_T)) {
        type.base = TYPE_UNSIGNED_LONG_LONG;
        c_eat(p, C_TOKEN_UINTMAX_T);
    } else if (c_match(p, C_TOKEN_NULLPTR_T)) {
        type.base = TYPE_VOID;
        c_eat(p, C_TOKEN_NULLPTR_T);
    } else if (c_match(p, C_TOKEN_MAX_ALIGN_T)) {
        type.base = TYPE_LONG_LONG;
        c_eat(p, C_TOKEN_MAX_ALIGN_T);
    } else if (c_match(p, C_TOKEN_IDENTIFIER)) {
        VarType typedef_type = c_lookup_typedef(p, p->current.text);
        if (typedef_type.base != TYPE_UNKNOWN) {
            type = typedef_type;
        } else {
            type.base = TYPE_CLASS;
            type.class_name = arena_strdup(p->ctx->arena, p->current.text);
        }
        c_eat(p, C_TOKEN_IDENTIFIER);
    } else {
        return (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    }

    while (c_match(p, C_TOKEN_STAR)) {
        c_eat(p, C_TOKEN_STAR);
        ptr_depth++;
        while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT)) {
            c_eat(p, p->current.type);
        }
        while (c_match(p, C_TOKEN_IDENTIFIER)) {
            if (strcmp(p->current.text, "__restrict") == 0 ||
                strcmp(p->current.text, "__volatile") == 0 ||
                strcmp(p->current.text, "__const") == 0 ||
                strcmp(p->current.text, "__restrict__") == 0) {
                c_eat(p, C_TOKEN_IDENTIFIER);
            } else {
                break;
            }
        }
    }

    while (c_match(p, C_TOKEN_LBRACKET)) {
        c_eat(p, C_TOKEN_LBRACKET);
        int depth = 1;
        while (depth > 0 && !p->has_error) {
            if (c_match(p, C_TOKEN_LBRACKET)) { depth++; c_eat(p, C_TOKEN_LBRACKET); }
            else if (c_match(p, C_TOKEN_RBRACKET)) { depth--; c_eat(p, C_TOKEN_RBRACKET); }
            else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
            else { c_eat(p, p->current.type); }
        }
    }

    if (c_match(p, C_TOKEN_LPAREN)) {
        c_eat(p, C_TOKEN_LPAREN);
        if (c_match(p, C_TOKEN_STAR)) {
            c_eat(p, C_TOKEN_STAR);
            type.is_func_ptr = 1;
            type.fp_ret_type = arena_alloc(p->ctx->arena, sizeof(VarType));
            *type.fp_ret_type = type;
            type.fp_param_count = 0;

            if (c_match(p, C_TOKEN_IDENTIFIER)) {
                c_eat(p, C_TOKEN_IDENTIFIER);
            }

            c_eat(p, C_TOKEN_RPAREN);

            c_eat(p, C_TOKEN_LPAREN);
            int cap = 16;
            type.fp_param_types = arena_alloc(p->ctx->arena, sizeof(VarType) * cap);
            while (!c_match(p, C_TOKEN_RPAREN) && !p->has_error) {
                VarType pt = c_parse_c_type(p, &ptr_depth, &array_size);
                (void)ptr_depth; (void)array_size;
                if (type.fp_param_count >= cap) {
                    cap *= 2;
                    VarType *new_arr = arena_alloc(p->ctx->arena, sizeof(VarType) * cap);
                    memcpy(new_arr, type.fp_param_types, sizeof(VarType) * type.fp_param_count);
                    type.fp_param_types = new_arr;
                }
                type.fp_param_types[type.fp_param_count++] = pt;
                if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);
                else if (c_match(p, C_TOKEN_ELLIPSIS)) {
                    type.fp_is_varargs = 1;
                    c_eat(p, C_TOKEN_ELLIPSIS);
                    break;
                }
                else break;
            }
            c_eat(p, C_TOKEN_RPAREN);

            type.base = TYPE_VOID;
            type.class_name = NULL;
            type.ptr_depth = 0;
            type.array_size = 0;
            if (out_ptr_depth) *out_ptr_depth = 0;
            if (out_array_size) *out_array_size = 0;
            return type;
        } else {
            int depth = 1;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) { depth++; c_eat(p, C_TOKEN_LPAREN); }
                else if (c_match(p, C_TOKEN_RPAREN)) { depth--; c_eat(p, C_TOKEN_RPAREN); }
                else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                else { c_eat(p, p->current.type); }
            }
        }
    }

    if (out_ptr_depth) *out_ptr_depth = ptr_depth;
    if (out_array_size) *out_array_size = array_size;
    type.ptr_depth = ptr_depth;
    type.array_size = array_size;
    return type;
}

static ASTNode* c_parse_extern_function(CParser *p) {
    char *cconv = NULL;
    char *extern_name = NULL;

    while (1) {
        if (c_match(p, C_TOKEN_STDCALL)) { cconv = arena_strdup(p->ctx->arena, "__stdcall"); c_eat(p, C_TOKEN_STDCALL); }
        else if (c_match(p, C_TOKEN_CDECL)) { cconv = arena_strdup(p->ctx->arena, "__cdecl"); c_eat(p, C_TOKEN_CDECL); }
        else if (c_match(p, C_TOKEN_FASTCALL)) { cconv = arena_strdup(p->ctx->arena, "__fastcall"); c_eat(p, C_TOKEN_FASTCALL); }
        else if (c_match(p, C_TOKEN_THISCALL)) { cconv = arena_strdup(p->ctx->arena, "__thiscall"); c_eat(p, C_TOKEN_THISCALL); }
        else if (c_match(p, C_TOKEN_VECTORCALL)) { cconv = arena_strdup(p->ctx->arena, "__vectorcall"); c_eat(p, C_TOKEN_VECTORCALL); }
        else if (c_match(p, C_TOKEN_DECLSPEC)) {
            c_eat(p, C_TOKEN_DECLSPEC);
            c_eat(p, C_TOKEN_LPAREN);
            int depth = 1;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) depth++;
                else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                c_eat(p, p->current.type);
            }
        }
        else if (c_match(p, C_TOKEN_ATTRIBUTE)) {
            c_eat(p, C_TOKEN_ATTRIBUTE);
            c_eat(p, C_TOKEN_LPAREN);
            c_eat(p, C_TOKEN_LPAREN);
            int depth = 2;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) depth++;
                else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                c_eat(p, p->current.type);
            }
        }
        else break;
    }

    c_eat(p, C_TOKEN_EXTERN);

    while (c_match(p, C_TOKEN_STRING)) {
        c_eat(p, C_TOKEN_STRING);
    }

    while (1) {
        if (c_match(p, C_TOKEN_STDCALL)) { cconv = arena_strdup(p->ctx->arena, "__stdcall"); c_eat(p, C_TOKEN_STDCALL); }
        else if (c_match(p, C_TOKEN_CDECL)) { cconv = arena_strdup(p->ctx->arena, "__cdecl"); c_eat(p, C_TOKEN_CDECL); }
        else if (c_match(p, C_TOKEN_FASTCALL)) { cconv = arena_strdup(p->ctx->arena, "__fastcall"); c_eat(p, C_TOKEN_FASTCALL); }
        else if (c_match(p, C_TOKEN_THISCALL)) { cconv = arena_strdup(p->ctx->arena, "__thiscall"); c_eat(p, C_TOKEN_THISCALL); }
        else if (c_match(p, C_TOKEN_VECTORCALL)) { cconv = arena_strdup(p->ctx->arena, "__vectorcall"); c_eat(p, C_TOKEN_VECTORCALL); }
        else if (c_match(p, C_TOKEN_DECLSPEC)) {
            c_eat(p, C_TOKEN_DECLSPEC);
            c_eat(p, C_TOKEN_LPAREN);
            int depth = 1;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) depth++;
                else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                c_eat(p, p->current.type);
            }
        }
        else if (c_match(p, C_TOKEN_ATTRIBUTE)) {
            c_eat(p, C_TOKEN_ATTRIBUTE);
            c_eat(p, C_TOKEN_LPAREN);
            c_eat(p, C_TOKEN_LPAREN);
            int depth = 2;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) depth++;
                else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                c_eat(p, p->current.type);
            }
        }
        else break;
    }

    int ptr_depth = 0;
    int array_size = 0;
    VarType ret_type = c_parse_c_type(p, &ptr_depth, &array_size);

    if (c_match(p, C_TOKEN_IDENTIFIER) && strcmp(p->current.text, "as") == 0) {
        c_eat(p, C_TOKEN_IDENTIFIER);
        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            extern_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);
        }
    }

    if (!c_match(p, C_TOKEN_IDENTIFIER)) {
        c_parser_error(p, "Expected function name in extern declaration");
        return NULL;
    }

    char *func_name = arena_strdup(p->ctx->arena, p->current.text);
    c_eat(p, C_TOKEN_IDENTIFIER);

    Parameter *params = NULL;
    Parameter **curr_param = &params;

    if (c_match(p, C_TOKEN_LPAREN)) {
        c_eat(p, C_TOKEN_LPAREN);
        if (!c_match(p, C_TOKEN_RPAREN)) {
            fprintf(stderr, "DEBUG params start: current=%s\n", p->current.text ? p->current.text : "NONAME");
            params = c_parse_parameters(p);
            fprintf(stderr, "DEBUG params end: current=%s\n", p->current.text ? p->current.text : "NONAME");
        }
        c_eat(p, C_TOKEN_RPAREN);
    }

    // Skip trailing annotations like __THROW, __attribute__((...)), __nonnull, __asm, etc.
    while (!c_match(p, C_TOKEN_SEMICOLON) && !p->has_error) {
        if (c_match(p, C_TOKEN_ATTRIBUTE)) {
            c_eat(p, C_TOKEN_ATTRIBUTE);
            c_eat(p, C_TOKEN_LPAREN);
            c_eat(p, C_TOKEN_LPAREN);
            int depth = 2;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) depth++;
                else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                c_eat(p, p->current.type);
            }
        } else if (c_match(p, C_TOKEN_ASM)) {
            c_eat(p, C_TOKEN_ASM);
            if (c_match(p, C_TOKEN_LPAREN)) {
                c_eat(p, C_TOKEN_LPAREN);
                int depth = 1;
                while (depth > 0 && !p->has_error) {
                    if (c_match(p, C_TOKEN_LPAREN)) depth++;
                    else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                    c_eat(p, p->current.type);
                }
            }
        } else if (c_match(p, C_TOKEN_IDENTIFIER)) {
            c_eat(p, C_TOKEN_IDENTIFIER);
        } else if (c_match(p, C_TOKEN_LPAREN)) {
            int depth = 1;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) { depth++; c_eat(p, C_TOKEN_LPAREN); }
                else if (c_match(p, C_TOKEN_RPAREN)) { depth--; c_eat(p, C_TOKEN_RPAREN); }
                else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                else { c_eat(p, p->current.type); }
            }
        } else {
            break;
        }
    }

    c_eat(p, C_TOKEN_SEMICOLON);

    FuncDefNode *func = arena_alloc(p->ctx->arena, sizeof(FuncDefNode));
    memset(func, 0, sizeof(FuncDefNode));
    func->base.type = NODE_FUNC_DEF;
    func->base.line = p->current.line;
    func->base.col = p->current.col;
    func->name = func_name;
    func->mangled_name = arena_strdup(p->ctx->arena, func_name);
    func->ret_type = ret_type;
    func->params = params;
    func->body = NULL;
    func->is_extern = 1;
    func->has_body = 0;
    func->cconv = cconv;
    func->extern_name = extern_name;

    return (ASTNode*)func;
}

static Parameter* c_parse_parameters(CParser *p) {
    Parameter *head = NULL;
    Parameter **curr = &head;

    while (!c_match(p, C_TOKEN_RPAREN) && !p->has_error) {
        fprintf(stderr, "DEBUG param: current=%s type=%d\n", p->current.text ? p->current.text : "NONAME", p->current.type);
        int ptr_depth = 0;
        int array_size = 0;
        VarType param_type = c_parse_c_type(p, &ptr_depth, &array_size);
        fprintf(stderr, "DEBUG after c_type: current=%s type=%d base=%d\n", p->current.text ? p->current.text : "NONAME", p->current.type, param_type.base);

        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            fprintf(stderr, "DEBUG consuming identifier: %s\n", p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);
        }

        while (c_match(p, C_TOKEN_LBRACKET)) {
            c_eat(p, C_TOKEN_LBRACKET);
            int depth = 1;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LBRACKET)) { depth++; c_eat(p, C_TOKEN_LBRACKET); }
                else if (c_match(p, C_TOKEN_RBRACKET)) { depth--; c_eat(p, C_TOKEN_RBRACKET); }
                else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                else { c_eat(p, p->current.type); }
            }
        }

        if (c_match(p, C_TOKEN_ASSIGN)) {
            c_eat(p, C_TOKEN_ASSIGN);
            while (!c_match(p, C_TOKEN_COMMA) && !c_match(p, C_TOKEN_RPAREN) && !p->has_error) {
                c_eat(p, p->current.type);
            }
        }

        Parameter *param = arena_alloc(p->ctx->arena, sizeof(Parameter));
        memset(param, 0, sizeof(Parameter));
        param->type = param_type;
        param->name = NULL;

        *curr = param;
        curr = &param->next;

        fprintf(stderr, "DEBUG before comma check: current=%s\n", p->current.text ? p->current.text : "NONAME");

        if (c_match(p, C_TOKEN_COMMA)) {
            c_eat(p, C_TOKEN_COMMA);
            if (c_match(p, C_TOKEN_ELLIPSIS)) {
                Parameter *vp = arena_alloc(p->ctx->arena, sizeof(Parameter));
                memset(vp, 0, sizeof(Parameter));
                vp->type.base = TYPE_UNKNOWN;
                c_eat(p, C_TOKEN_ELLIPSIS);
                *curr = vp;
                break;
            }
        } else {
            fprintf(stderr, "DEBUG breaking from param loop, current=%s\n", p->current.text ? p->current.text : "NONAME");
            break;
        }
    }

    return head;
}

static ASTNode* c_parse_struct_or_union(CParser *p, int is_union) {
    c_eat(p, is_union ? C_TOKEN_UNION : C_TOKEN_STRUCT);

    char *name = NULL;
    if (c_match(p, C_TOKEN_IDENTIFIER)) {
        name = arena_strdup(p->ctx->arena, p->current.text);
        c_eat(p, C_TOKEN_IDENTIFIER);
    } else {
        static int anon_count = 0;
        char buf[64];
        snprintf(buf, sizeof(buf), "__anon_%s_%d", is_union ? "union" : "struct", ++anon_count);
        name = arena_strdup(p->ctx->arena, buf);
    }

    if (!c_match(p, C_TOKEN_LBRACE)) {
        c_eat(p, C_TOKEN_SEMICOLON);
        StructNode *sn = arena_alloc(p->ctx->arena, sizeof(StructNode));
        memset(sn, 0, sizeof(StructNode));
        sn->base.type = NODE_STRUCT;
        sn->base.line = p->current.line;
        sn->base.col = p->current.col;
        sn->name = name;
        sn->is_union = is_union;
        sn->is_extern = 1;
        sn->has_body = 0;
        return (ASTNode*)sn;
    }

    c_eat(p, C_TOKEN_LBRACE);

    StructNode *sn = arena_alloc(p->ctx->arena, sizeof(StructNode));
    memset(sn, 0, sizeof(StructNode));
    sn->base.type = NODE_STRUCT;
    sn->base.line = p->current.line;
    sn->base.col = p->current.col;
    sn->name = name;
    sn->is_union = is_union;
    sn->has_body = 1;
    sn->is_extern = 1;

    ASTNode **curr_member = &sn->members;

    while (!c_match(p, C_TOKEN_RBRACE) && !p->has_error) {
        while (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION) || c_match(p, C_TOKEN_ENUM)) {
            c_eat(p, p->current.type);
        }

        while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT) || c_match(p, C_TOKEN_EXTENSION)) {
            c_eat(p, p->current.type);
        }
        while (c_match(p, C_TOKEN_IDENTIFIER) && strcmp(p->current.text, "__extension__") == 0) {
            c_eat(p, C_TOKEN_IDENTIFIER);
        }

        int ptr_depth = 0;
        int array_size = 0;
        VarType member_type = c_parse_c_type(p, &ptr_depth, &array_size);

        if (member_type.base == TYPE_UNKNOWN) {
            c_parser_error(p, "Unknown type in struct/union member");
            while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_RBRACE) && !p->has_error) {
                c_eat(p, p->current.type);
            }
            if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
            continue;
        }

        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            char *member_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);

            while (c_match(p, C_TOKEN_LBRACKET)) {
                c_eat(p, C_TOKEN_LBRACKET);
                int depth = 1;
                while (depth > 0 && !p->has_error) {
                    if (c_match(p, C_TOKEN_LBRACKET)) { depth++; c_eat(p, C_TOKEN_LBRACKET); }
                    else if (c_match(p, C_TOKEN_RBRACKET)) { depth--; c_eat(p, C_TOKEN_RBRACKET); }
                    else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                    else { c_eat(p, p->current.type); }
                }
            }

            if (c_match(p, C_TOKEN_COLON)) {
                c_eat(p, C_TOKEN_COLON);
                if (c_match(p, C_TOKEN_NUMBER)) {
                    c_eat(p, C_TOKEN_NUMBER);
                }
            }

            VarDeclNode *var = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
            memset(var, 0, sizeof(VarDeclNode));
            var->base.type = NODE_VAR_DECL;
            var->base.line = p->current.line;
            var->base.col = p->current.col;
            var->name = member_name;
            var->var_type = member_type;
            var->var_type.ptr_depth += ptr_depth;
            var->var_type.array_size = array_size > 0 ? array_size : member_type.array_size;
            var->is_array = array_size > 0;
            var->is_mutable = 1;

            *curr_member = (ASTNode*)var;
            curr_member = &var->base.next;
        }

        c_eat(p, C_TOKEN_SEMICOLON);
    }

    c_eat(p, C_TOKEN_RBRACE);

    ASTNode *post_decl = NULL;
    if (c_match(p, C_TOKEN_IDENTIFIER)) {
        char *var_name = arena_strdup(p->ctx->arena, p->current.text);
        c_eat(p, C_TOKEN_IDENTIFIER);

        VarDeclNode *var = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
        memset(var, 0, sizeof(VarDeclNode));
        var->base.type = NODE_VAR_DECL;
        var->base.line = p->current.line;
        var->base.col = p->current.col;
        var->name = var_name;
        var->var_type.base = TYPE_CLASS;
        var->var_type.class_name = arena_strdup(p->ctx->arena, name);
        var->var_type.ptr_depth = 0;
        var->is_mutable = 1;
        post_decl = (ASTNode*)var;

        while (c_match(p, C_TOKEN_LBRACKET)) {
            c_eat(p, C_TOKEN_LBRACKET);
            int depth = 1;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LBRACKET)) { depth++; c_eat(p, C_TOKEN_LBRACKET); }
                else if (c_match(p, C_TOKEN_RBRACKET)) { depth--; c_eat(p, C_TOKEN_RBRACKET); }
                else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                else { c_eat(p, p->current.type); }
            }
        }
    }

    c_eat(p, C_TOKEN_SEMICOLON);

    if (post_decl) {
        ASTNode *last = (ASTNode*)sn;
        while (last->next) last = last->next;
        last->next = post_decl;
    }

    return (ASTNode*)sn;
}

static ASTNode* c_parse_enum(CParser *p) {
    c_eat(p, C_TOKEN_ENUM);

    char *name = NULL;
    if (c_match(p, C_TOKEN_IDENTIFIER)) {
        name = arena_strdup(p->ctx->arena, p->current.text);
        c_eat(p, C_TOKEN_IDENTIFIER);
    } else {
        static int anon_count = 0;
        char buf[64];
        snprintf(buf, sizeof(buf), "__anon_enum_%d", ++anon_count);
        name = arena_strdup(p->ctx->arena, buf);
    }

    if (!c_match(p, C_TOKEN_LBRACE)) {
        c_eat(p, C_TOKEN_SEMICOLON);
        EnumNode *en = arena_alloc(p->ctx->arena, sizeof(EnumNode));
        memset(en, 0, sizeof(EnumNode));
        en->base.type = NODE_ENUM;
        en->base.line = p->current.line;
        en->base.col = p->current.col;
        en->name = name;
        return (ASTNode*)en;
    }

    c_eat(p, C_TOKEN_LBRACE);

    EnumNode *en = arena_alloc(p->ctx->arena, sizeof(EnumNode));
    memset(en, 0, sizeof(EnumNode));
    en->base.type = NODE_ENUM;
    en->base.line = p->current.line;
    en->base.col = p->current.col;
    en->name = name;

    EnumEntry **curr_entry = &en->entries;
    int value = 0;

    while (!c_match(p, C_TOKEN_RBRACE) && !p->has_error) {
        if (!c_match(p, C_TOKEN_IDENTIFIER)) {
            c_parser_error(p, "Expected enumerator name");
            break;
        }

        char *entry_name = arena_strdup(p->ctx->arena, p->current.text);
        c_eat(p, C_TOKEN_IDENTIFIER);

        if (c_match(p, C_TOKEN_ASSIGN)) {
            c_eat(p, C_TOKEN_ASSIGN);
            if (c_match(p, C_TOKEN_NUMBER)) {
                value = (int)p->current.int_val;
                c_eat(p, C_TOKEN_NUMBER);
            } else if (c_match(p, C_TOKEN_IDENTIFIER)) {
                c_eat(p, C_TOKEN_IDENTIFIER);
            }
        }

        EnumEntry *entry = arena_alloc(p->ctx->arena, sizeof(EnumEntry));
        memset(entry, 0, sizeof(EnumEntry));
        entry->name = entry_name;
        entry->value = value++;

        *curr_entry = entry;
        curr_entry = &entry->next;

        if (c_match(p, C_TOKEN_COMMA)) {
            c_eat(p, C_TOKEN_COMMA);
        } else {
            break;
        }
    }

    c_eat(p, C_TOKEN_RBRACE);
    c_eat(p, C_TOKEN_SEMICOLON);

    return (ASTNode*)en;
}

static ASTNode* c_parse_typedef(CParser *p) {
    c_eat(p, C_TOKEN_TYPEDEF);

    if (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION) || c_match(p, C_TOKEN_ENUM)) {
        int is_union = c_match(p, C_TOKEN_UNION);
        c_eat(p, p->current.type);

        char *tag_name = NULL;
        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            tag_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);
        }

        if (c_match(p, C_TOKEN_LBRACE)) {
            c_eat(p, C_TOKEN_LBRACE);

            StructNode *sn = arena_alloc(p->ctx->arena, sizeof(StructNode));
            memset(sn, 0, sizeof(StructNode));
            sn->base.type = NODE_STRUCT;
            sn->base.line = p->current.line;
            sn->base.col = p->current.col;
            sn->name = tag_name ? tag_name : arena_strdup(p->ctx->arena, "__anon_typedef");
            sn->is_union = is_union;
            sn->has_body = 1;
            sn->is_extern = 1;

            ASTNode **curr_member = &sn->members;

            while (!c_match(p, C_TOKEN_RBRACE) && !p->has_error) {
                while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE)) {
                    c_eat(p, p->current.type);
                }

                int ptr_depth = 0;
                int array_size = 0;
                VarType member_type = c_parse_c_type(p, &ptr_depth, &array_size);

                if (member_type.base != TYPE_UNKNOWN && c_match(p, C_TOKEN_IDENTIFIER)) {
                    char *member_name = arena_strdup(p->ctx->arena, p->current.text);
                    c_eat(p, C_TOKEN_IDENTIFIER);

                    while (c_match(p, C_TOKEN_LBRACKET)) {
                        c_eat(p, C_TOKEN_LBRACKET);
                        if (c_match(p, C_TOKEN_NUMBER)) c_eat(p, C_TOKEN_NUMBER);
                        else if (c_match(p, C_TOKEN_IDENTIFIER)) c_eat(p, C_TOKEN_IDENTIFIER);
                        c_eat(p, C_TOKEN_RBRACKET);
                    }

                    VarDeclNode *var = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
                    memset(var, 0, sizeof(VarDeclNode));
                    var->base.type = NODE_VAR_DECL;
                    var->base.line = p->current.line;
                    var->base.col = p->current.col;
                    var->name = member_name;
                    var->var_type = member_type;
                    var->var_type.ptr_depth += ptr_depth;
                    var->var_type.array_size = array_size > 0 ? array_size : member_type.array_size;
                    var->is_mutable = 1;

                    *curr_member = (ASTNode*)var;
                    curr_member = &var->base.next;
                }

                c_eat(p, C_TOKEN_SEMICOLON);
            }

            c_eat(p, C_TOKEN_RBRACE);

            if (c_match(p, C_TOKEN_IDENTIFIER)) {
                char *typedef_name = arena_strdup(p->ctx->arena, p->current.text);
                c_eat(p, C_TOKEN_IDENTIFIER);
                VarType typedef_type;
                typedef_type.base = TYPE_CLASS;
                typedef_type.class_name = arena_strdup(p->ctx->arena, sn->name);
                typedef_type.ptr_depth = 0;
                c_register_typedef(p, typedef_name, typedef_type);
            }

            c_eat(p, C_TOKEN_SEMICOLON);
            return (ASTNode*)sn;
        } else {
            int ptr_depth = 0;
            while (c_match(p, C_TOKEN_STAR)) {
                c_eat(p, C_TOKEN_STAR);
                ptr_depth++;
                while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT)) {
                    c_eat(p, p->current.type);
                }
                while (c_match(p, C_TOKEN_IDENTIFIER)) {
                    if (strcmp(p->current.text, "__restrict") == 0 ||
                        strcmp(p->current.text, "__volatile") == 0 ||
                        strcmp(p->current.text, "__const") == 0 ||
                        strcmp(p->current.text, "__restrict__") == 0) {
                        c_eat(p, C_TOKEN_IDENTIFIER);
                    } else {
                        break;
                    }
                }
            }

            if (c_match(p, C_TOKEN_IDENTIFIER)) {
                char *typedef_name = arena_strdup(p->ctx->arena, p->current.text);
                c_eat(p, C_TOKEN_IDENTIFIER);
                VarType typedef_type;
                typedef_type.base = TYPE_CLASS;
                typedef_type.class_name = tag_name ? arena_strdup(p->ctx->arena, tag_name) : arena_strdup(p->ctx->arena, "__unknown");
                typedef_type.ptr_depth = ptr_depth;
                c_register_typedef(p, typedef_name, typedef_type);
            }
            c_eat(p, C_TOKEN_SEMICOLON);
            return NULL;
        }
    }

    int ptr_depth = 0;
    int array_size = 0;
    VarType base_type = c_parse_c_type(p, &ptr_depth, &array_size);

    if (c_match(p, C_TOKEN_IDENTIFIER)) {
        char *typedef_name = arena_strdup(p->ctx->arena, p->current.text);
        c_eat(p, C_TOKEN_IDENTIFIER);

        while (c_match(p, C_TOKEN_LBRACKET)) {
            c_eat(p, C_TOKEN_LBRACKET);
            int depth = 1;
            while (depth > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LBRACKET)) { depth++; c_eat(p, C_TOKEN_LBRACKET); }
                else if (c_match(p, C_TOKEN_RBRACKET)) { depth--; c_eat(p, C_TOKEN_RBRACKET); }
                else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                else { c_eat(p, p->current.type); }
            }
        }

        if (c_match(p, C_TOKEN_LPAREN)) {
            if (!c_match(p, C_TOKEN_STAR)) {
                int depth2 = 1;
                while (depth2 > 0 && !p->has_error) {
                    if (c_match(p, C_TOKEN_LPAREN)) { depth2++; c_eat(p, C_TOKEN_LPAREN); }
                    else if (c_match(p, C_TOKEN_RPAREN)) { depth2--; c_eat(p, C_TOKEN_RPAREN); }
                    else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                    else { c_eat(p, p->current.type); }
                }
            } else {
                c_eat(p, C_TOKEN_STAR);
                char *fp_name = NULL;
                if (c_match(p, C_TOKEN_IDENTIFIER)) {
                    fp_name = arena_strdup(p->ctx->arena, p->current.text);
                    c_eat(p, C_TOKEN_IDENTIFIER);
                }
                c_eat(p, C_TOKEN_RPAREN);

                Parameter *params = NULL;
                Parameter **curr_param = &params;
                if (c_match(p, C_TOKEN_LPAREN)) {
                    c_eat(p, C_TOKEN_LPAREN);
                    if (!c_match(p, C_TOKEN_RPAREN)) {
                        params = c_parse_parameters(p);
                    }
                    c_eat(p, C_TOKEN_RPAREN);
                }

                VarType fp_type;
                memset(&fp_type, 0, sizeof(VarType));
                fp_type.base = TYPE_VOID;
                fp_type.is_func_ptr = 1;
                fp_type.fp_ret_type = arena_alloc(p->ctx->arena, sizeof(VarType));
                *fp_type.fp_ret_type = base_type;
                fp_type.fp_param_count = 0;
                fp_type.fp_param_types = NULL;

                if (fp_name) {
                    c_register_typedef(p, fp_name, fp_type);
                }
            }
        }

        VarType typedef_type = base_type;
        typedef_type.ptr_depth += ptr_depth;
        typedef_type.array_size = array_size > 0 ? array_size : base_type.array_size;
        c_register_typedef(p, typedef_name, typedef_type);
    } else if (c_match(p, C_TOKEN_LPAREN)) {
        if (!c_match(p, C_TOKEN_STAR)) {
            int depth2 = 1;
            while (depth2 > 0 && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) { depth2++; c_eat(p, C_TOKEN_LPAREN); }
                else if (c_match(p, C_TOKEN_RPAREN)) { depth2--; c_eat(p, C_TOKEN_RPAREN); }
                else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                else { c_eat(p, p->current.type); }
            }
        } else {
            c_eat(p, C_TOKEN_STAR);
            char *fp_name = NULL;
            if (c_match(p, C_TOKEN_IDENTIFIER)) {
                fp_name = arena_strdup(p->ctx->arena, p->current.text);
                c_eat(p, C_TOKEN_IDENTIFIER);
            }
            c_eat(p, C_TOKEN_RPAREN);

            Parameter *params = NULL;
            Parameter **curr_param = &params;
            if (c_match(p, C_TOKEN_LPAREN)) {
                c_eat(p, C_TOKEN_LPAREN);
                if (!c_match(p, C_TOKEN_RPAREN)) {
                    params = c_parse_parameters(p);
                }
                c_eat(p, C_TOKEN_RPAREN);
            }

            VarType fp_type;
            memset(&fp_type, 0, sizeof(VarType));
            fp_type.base = TYPE_VOID;
            fp_type.is_func_ptr = 1;
            fp_type.fp_ret_type = arena_alloc(p->ctx->arena, sizeof(VarType));
            *fp_type.fp_ret_type = base_type;
            fp_type.fp_param_count = 0;
            fp_type.fp_param_types = NULL;

            if (fp_name) {
                c_register_typedef(p, fp_name, fp_type);
            }
        }
    }

    c_eat(p, C_TOKEN_SEMICOLON);
    return NULL;
}

static ASTNode* c_parse_extern_variable(CParser *p) {
    c_eat(p, C_TOKEN_EXTERN);

    int ptr_depth = 0;
    int array_size = 0;
    VarType var_type = c_parse_c_type(p, &ptr_depth, &array_size);

    if (!c_match(p, C_TOKEN_IDENTIFIER)) {
        c_parser_error(p, "Expected variable name in extern declaration");
        return NULL;
    }

    char *var_name = arena_strdup(p->ctx->arena, p->current.text);
    c_eat(p, C_TOKEN_IDENTIFIER);

    c_eat(p, C_TOKEN_SEMICOLON);

    VarDeclNode *var = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
    memset(var, 0, sizeof(VarDeclNode));
    var->base.type = NODE_VAR_DECL;
    var->base.line = p->current.line;
    var->base.col = p->current.col;
    var->name = var_name;
    var->var_type = var_type;
    var->var_type.ptr_depth += ptr_depth;
    var->var_type.array_size = array_size > 0 ? array_size : var_type.array_size;
    var->is_array = array_size > 0;
    var->is_mutable = 1;

    return (ASTNode*)var;
}

static ASTNode* c_parse_declaration(CParser *p) {
    if (c_match(p, C_TOKEN_TYPEDEF)) {
        return c_parse_typedef(p);
    }

    if (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION)) {
        return c_parse_struct_or_union(p, c_match(p, C_TOKEN_UNION));
    }

    if (c_match(p, C_TOKEN_ENUM)) {
        return c_parse_enum(p);
    }

    if (c_match(p, C_TOKEN_EXTERN)) {
        CLexer save_lexer = p->lexer;
        CToken save_current = p->current;
        int save_error = p->has_error;

        c_eat(p, C_TOKEN_EXTERN);

        while (1) {
            if (c_match(p, C_TOKEN_STDCALL) || c_match(p, C_TOKEN_CDECL) || c_match(p, C_TOKEN_FASTCALL) ||
                c_match(p, C_TOKEN_THISCALL) || c_match(p, C_TOKEN_VECTORCALL)) {
                c_eat(p, p->current.type);
            } else if (c_match(p, C_TOKEN_DECLSPEC)) {
                c_eat(p, C_TOKEN_DECLSPEC);
                c_eat(p, C_TOKEN_LPAREN);
                int depth = 1;
                while (depth > 0 && !p->has_error) {
                    if (c_match(p, C_TOKEN_LPAREN)) depth++;
                    else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                    c_eat(p, p->current.type);
                }
            } else if (c_match(p, C_TOKEN_ATTRIBUTE)) {
                c_eat(p, C_TOKEN_ATTRIBUTE);
                c_eat(p, C_TOKEN_LPAREN);
                c_eat(p, C_TOKEN_LPAREN);
                int depth = 2;
                while (depth > 0 && !p->has_error) {
                    if (c_match(p, C_TOKEN_LPAREN)) depth++;
                    else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                    c_eat(p, p->current.type);
                }
            } else {
                break;
            }
        }

        while (c_match(p, C_TOKEN_STRING)) {
            c_eat(p, C_TOKEN_STRING);
        }

        int is_function = 0;
        if (c_match(p, C_TOKEN_VOID) || c_match(p, C_TOKEN_CHAR_KW) || c_match(p, C_TOKEN_SHORT) ||
            c_match(p, C_TOKEN_INT) || c_match(p, C_TOKEN_LONG) || c_match(p, C_TOKEN_FLOAT) ||
            c_match(p, C_TOKEN_DOUBLE) || c_match(p, C_TOKEN_SIGNED) || c_match(p, C_TOKEN_UNSIGNED) ||
            c_match(p, C_TOKEN_BOOL) || c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION) ||
            c_match(p, C_TOKEN_ENUM) || c_match(p, C_TOKEN_SIZE_T) || c_match(p, C_TOKEN_PTRDIFF_T) ||
            c_match(p, C_TOKEN_WCHAR_T) || c_match(p, C_TOKEN_IDENTIFIER)) {
            c_eat(p, p->current.type);
            while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT)) c_eat(p, p->current.type);
            while (c_match(p, C_TOKEN_STAR)) c_eat(p, C_TOKEN_STAR);
            // Consume multi-word type specifiers like "long int", "unsigned long", "short int", "signed char", "long double"
            while (c_match(p, C_TOKEN_SHORT) || c_match(p, C_TOKEN_INT) || c_match(p, C_TOKEN_LONG) ||
                   c_match(p, C_TOKEN_CHAR_KW) || c_match(p, C_TOKEN_SIGNED) || c_match(p, C_TOKEN_UNSIGNED) ||
                   c_match(p, C_TOKEN_DOUBLE) || c_match(p, C_TOKEN_FLOAT)) {
                c_eat(p, p->current.type);
                while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT)) c_eat(p, p->current.type);
                while (c_match(p, C_TOKEN_STAR)) c_eat(p, C_TOKEN_STAR);
            }
            while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT)) c_eat(p, p->current.type);
            while (c_match(p, C_TOKEN_STAR)) c_eat(p, C_TOKEN_STAR);
            if (c_match(p, C_TOKEN_IDENTIFIER)) {
                c_eat(p, C_TOKEN_IDENTIFIER);
                if (c_match(p, C_TOKEN_LPAREN)) {
                    is_function = 1;
                }
            }
        }

        p->lexer = save_lexer;
        p->current = save_current;
        p->has_error = save_error;

        if (is_function) {
            return c_parse_extern_function(p);
        } else {
            return c_parse_extern_variable(p);
        }
    }

    while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
        c_eat(p, p->current.type);
    }
    if (c_match(p, C_TOKEN_SEMICOLON)) {
        c_eat(p, C_TOKEN_SEMICOLON);
    }

    return NULL;
}

void c_parser_init(CParser *p, CompilerContext *ctx, const char *filename, const char *source) {
    memset(p, 0, sizeof(CParser));
    c_lexer_init(&p->lexer, ctx, filename, source);
    p->ctx = ctx;
    p->current = c_lexer_next(&p->lexer);
    p->typedefs.capacity = 64;
    p->typedefs.names = arena_alloc(ctx->arena, sizeof(char*) * p->typedefs.capacity);
    p->typedefs.types = arena_alloc(ctx->arena, sizeof(VarType) * p->typedefs.capacity);
    p->defines.capacity = 64;
    p->defines.names = arena_alloc(ctx->arena, sizeof(char*) * p->defines.capacity);
    p->defines.values = arena_alloc(ctx->arena, sizeof(char*) * p->defines.capacity);
    p->cond_stack.capacity = 32;
    p->cond_stack.active = arena_alloc(ctx->arena, sizeof(int) * p->cond_stack.capacity);
}

ASTNode* c_parse_header(CParser *p) {
    ASTNode *head = NULL;
    ASTNode **curr = &head;

    while (!c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
        ASTNode *decl = c_parse_declaration(p);
        if (decl) {
            *curr = decl;
            while (*curr) curr = &(*curr)->next;
        }
    }

    return head;
}
