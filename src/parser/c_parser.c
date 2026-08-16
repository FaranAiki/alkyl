/**
 * @file c_parser.c
 * @brief C header file parser implementation for the Alkyl compiler.
 */
#include "c_parser.h"
#include "parser.h"
#include "typestruct.h"
#include "../common/diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void c_parser_error(CParser *p, const char *msg) {
    report_c_error(&p->lexer, p->current, msg);
    p->has_error = 1;
}

static void c_eat(CParser *p, CTokenType type) {
    if (p->current.type == type) {
        p->current = c_lexer_next(&p->lexer);
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "Expected %s but found %s",
                 c_token_type_to_string(type),
                 p->current.text ? p->current.text : c_token_type_to_string(p->current.type));
        c_parser_error(p, buf);
        p->current = c_lexer_next(&p->lexer);
    }
}

static int c_match(CParser *p, CTokenType type) {
    return p->current.type == type;
}

static void c_register_typedef(CParser *p, const char *name, VarType type) {
    if (!name) return;
    VarType *vt = p->ctx ? arena_alloc(p->ctx->arena, sizeof(VarType)) : calloc(1, sizeof(VarType));
    *vt = type;
    hashmap_put(&p->typedef_map, name, vt);
}

static BaseType c_identifier_to_base_type(const char *name);

static VarType c_lookup_typedef(CParser *p, const char *name) {
    if (!name) return (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    VarType *vt = (VarType*)hashmap_get(&p->typedef_map, name);
    if (vt) return *vt;
    BaseType bt = c_identifier_to_base_type(name);
    if (bt != TYPE_UNKNOWN) return (VarType){bt, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    return (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
}

static BaseType c_identifier_to_base_type(const char *name) {
    if (!name) return TYPE_UNKNOWN;
    if (streq_lit(name, "size_t")) return TYPE_UNSIGNED_LONG;
    if (streq_lit(name, "ptrdiff_t")) return TYPE_LONG;
    if (streq_lit(name, "off_t") || streq_lit(name, "__off_t")) return TYPE_LONG;
    if (streq_lit(name, "off64_t") || streq_lit(name, "__off64_t")) return TYPE_LONG_LONG;
    if (streq_lit(name, "ssize_t") || streq_lit(name, "__ssize_t")) return TYPE_LONG;
    if (streq_lit(name, "wchar_t")) return TYPE_SHORT;
    if (streq_lit(name, "char16_t")) return TYPE_SHORT;
    if (streq_lit(name, "char32_t")) return TYPE_INT;
    if (streq_lit(name, "int8_t") || streq_lit(name, "int_least8_t") || streq_lit(name, "int_fast8_t") || streq_lit(name, "__int8_t")) return TYPE_CHAR;
    if (streq_lit(name, "uint8_t") || streq_lit(name, "uint_least8_t") || streq_lit(name, "uint_fast8_t") || streq_lit(name, "__uint8_t")) return TYPE_UNSIGNED_CHAR;
    if (streq_lit(name, "int16_t") || streq_lit(name, "int_least16_t") || streq_lit(name, "int_fast16_t") || streq_lit(name, "__int16_t")) return TYPE_SHORT;
    if (streq_lit(name, "uint16_t") || streq_lit(name, "uint_least16_t") || streq_lit(name, "uint_fast16_t") || streq_lit(name, "__uint16_t")) return TYPE_UNSIGNED_INT;
    if (streq_lit(name, "int32_t") || streq_lit(name, "int_least32_t") || streq_lit(name, "int_fast32_t") || streq_lit(name, "intmax_t") || streq_lit(name, "__int32_t")) return TYPE_INT;
    if (streq_lit(name, "uint32_t") || streq_lit(name, "uint_least32_t") || streq_lit(name, "uint_fast32_t") || streq_lit(name, "uintmax_t") || streq_lit(name, "__uint32_t")) return TYPE_UNSIGNED_INT;
    if (streq_lit(name, "int64_t") || streq_lit(name, "int_least64_t") || streq_lit(name, "int_fast64_t") || streq_lit(name, "__int64_t")) return TYPE_LONG_LONG;
    if (streq_lit(name, "uint64_t") || streq_lit(name, "uint_least64_t") || streq_lit(name, "uint_fast64_t") || streq_lit(name, "__uint64_t")) return TYPE_UNSIGNED_LONG_LONG;
    if (streq_lit(name, "max_align_t")) return TYPE_LONG_LONG;
    if (streq_lit(name, "nullptr_t")) return TYPE_VOID;
    if (streq_lit(name, "__gnuc_va_list") || streq_lit(name, "__builtin_va_list")) return TYPE_VOID;
    if (streq_lit(name, "FILE") || streq_lit(name, "_IO_FILE") || streq_lit(name, "__FILE")) return TYPE_CLASS;
    if (streq_lit(name, "__SIZE_TYPE__") || streq_lit(name, "__UINTPTR_TYPE__")) return TYPE_UNSIGNED_LONG;
    if (streq_lit(name, "__PTRDIFF_TYPE__") || streq_lit(name, "__INTPTR_TYPE__")) return TYPE_LONG;
    if (streq_lit(name, "__WCHAR_TYPE__")) return TYPE_INT;
    if (streq_lit(name, "__WINT_TYPE__") || streq_lit(name, "wint_t") || streq_lit(name, "__wint_t")) return TYPE_UNSIGNED_INT;
    if (streq_lit(name, "mbstate_t") || streq_lit(name, "__mbstate_t")) return TYPE_CLASS;
    if (streq_lit(name, "__float128") || streq_lit(name, "_Float128") || streq_lit(name, "_Float32") ||
        streq_lit(name, "_Float64") || streq_lit(name, "_Float32x") || streq_lit(name, "_Float64x") ||
        streq_lit(name, "_Float128x") || streq_lit(name, "__float80") || streq_lit(name, "__ibm128")) return TYPE_DOUBLE;
    if (streq_lit(name, "__int128") || streq_lit(name, "__int128_t")) return TYPE_LONG_LONG;
    if (streq_lit(name, "__uint128_t")) return TYPE_UNSIGNED_LONG_LONG;
    return TYPE_UNKNOWN;
}

static int c_is_type_token(CParser *p, CToken tok) {
    if (tok.type == C_TOKEN_VOID || tok.type == C_TOKEN_CHAR_KW || tok.type == C_TOKEN_SHORT ||
        tok.type == C_TOKEN_INT || tok.type == C_TOKEN_LONG || tok.type == C_TOKEN_FLOAT ||
        tok.type == C_TOKEN_DOUBLE || tok.type == C_TOKEN_SIGNED || tok.type == C_TOKEN_UNSIGNED ||
        tok.type == C_TOKEN_BOOL || tok.type == C_TOKEN_BOOL_KA || tok.type == C_TOKEN_STRUCT ||
        tok.type == C_TOKEN_UNION || tok.type == C_TOKEN_ENUM || tok.type == C_TOKEN_COMPLEX) {
        return 1;
    }
    if (tok.type == C_TOKEN_IDENTIFIER && tok.text) {
        if (c_lookup_typedef(p, tok.text).base != TYPE_UNKNOWN ||
            c_identifier_to_base_type(tok.text) != TYPE_UNKNOWN) {
            return 1;
        }
    }
    return 0;
}

static void c_skip_modifiers(CParser *p) {
    while (!p->has_error && p->current.type != C_TOKEN_EOF) {
        if (c_match(p, C_TOKEN_TYPEDEF) || c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION) || c_match(p, C_TOKEN_ENUM)) {
            break;
        }
        if (c_match(p, C_TOKEN_EXTERN) || c_match(p, C_TOKEN_STATIC) || c_match(p, C_TOKEN_CONST) ||
            c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT) || c_match(p, C_TOKEN_EXTENSION) ||
            c_match(p, C_TOKEN_STDCALL) || c_match(p, C_TOKEN_CDECL) || c_match(p, C_TOKEN_FASTCALL) ||
            c_match(p, C_TOKEN_THISCALL) || c_match(p, C_TOKEN_VECTORCALL) || c_match(p, C_TOKEN_INLINE)) {
            c_eat(p, p->current.type);
        } else if (c_match(p, C_TOKEN_DECLSPEC)) {
            c_eat(p, C_TOKEN_DECLSPEC);
            if (c_match(p, C_TOKEN_LPAREN)) {
                c_eat(p, C_TOKEN_LPAREN);
                int depth = 1;
                while (depth > 0 && !p->has_error && p->current.type != C_TOKEN_EOF) {
                    if (c_match(p, C_TOKEN_LPAREN)) depth++;
                    else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                    c_eat(p, p->current.type);
                }
            }
        } else if (c_match(p, C_TOKEN_ATTRIBUTE)) {
            c_eat(p, C_TOKEN_ATTRIBUTE);
            if (c_match(p, C_TOKEN_LPAREN)) {
                c_eat(p, C_TOKEN_LPAREN);
                int depth = 1;
                while (depth > 0 && !p->has_error && p->current.type != C_TOKEN_EOF) {
                    if (c_match(p, C_TOKEN_LPAREN)) depth++;
                    else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                    c_eat(p, p->current.type);
                }
            }
        } else if (c_match(p, C_TOKEN_ASM)) {
            c_eat(p, C_TOKEN_ASM);
            if (c_match(p, C_TOKEN_LPAREN)) {
                c_eat(p, C_TOKEN_LPAREN);
                int depth = 1;
                while (depth > 0 && !p->has_error && p->current.type != C_TOKEN_EOF) {
                    if (c_match(p, C_TOKEN_LPAREN)) depth++;
                    else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                    c_eat(p, p->current.type);
                }
            }
        } else if (c_match(p, C_TOKEN_IDENTIFIER)) {
            const char *txt = p->current.text;
            CLexer look_l = p->lexer;
            CToken look_tok = c_lexer_next(&look_l);

            int is_attr_prefix = (strncmp(txt, "__attribute", 11) == 0 ||
                                  strncmp(txt, "__attr_", 7) == 0 ||
                                  strncmp(txt, "__fortif", 8) == 0 ||
                                  (strlen(txt) > 5 && (streq(txt + strlen(txt) - 5, "_DECL") || streq(txt + strlen(txt) - 6, "_DECLS")) && strncmp(txt, "__LDBL_REDIR", 12) != 0 && strncmp(txt, "__REDIRECT", 10) != 0) ||
                                  streq_lit(txt, "__BEGIN_DECLS") ||
                                  streq_lit(txt, "__END_DECLS") ||
                                  streq_lit(txt, "__nonnull") ||
                                  streq_lit(txt, "__warnattr") ||
                                  streq_lit(txt, "__errordecl") ||
                                  streq_lit(txt, "__warndecl"));

            if (look_tok.type == C_TOKEN_LPAREN) {
                if (is_attr_prefix) {
                    c_eat(p, C_TOKEN_IDENTIFIER);
                    c_eat(p, C_TOKEN_LPAREN);
                    int depth = 1;
                    while (depth > 0 && !p->has_error && p->current.type != C_TOKEN_EOF) {
                        if (c_match(p, C_TOKEN_LPAREN)) depth++;
                        else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                        c_eat(p, p->current.type);
                    }
                } else {
                    break;
                }
            } else if (is_attr_prefix ||
                       streq_lit(txt, "_Complex") || streq_lit(txt, "__complex__") || streq_lit(txt, "__complex") ||
                       streq_lit(txt, "__COLD") || streq_lit(txt, "__cold__") || streq_lit(txt, "__HOT") || streq_lit(txt, "__hot__") ||
                       streq_lit(txt, "__extension__") || streq_lit(txt, "inline") || streq_lit(txt, "__inline") ||
                       streq_lit(txt, "__inline__") || streq_lit(txt, "__always_inline") || streq_lit(txt, "__extern_inline") ||
                       streq_lit(txt, "__extern_always_inline") || streq_lit(txt, "__fortify_function") ||
                       streq_lit(txt, "__fortify_function_error_function") || streq_lit(txt, "__wur") ||
                       streq_lit(txt, "__THROW") || streq_lit(txt, "__THROWNL") || streq_lit(txt, "__leaf") ||
                       streq_lit(txt, "__restrict") || streq_lit(txt, "__restrict__") || streq_lit(txt, "__const") ||
                       streq_lit(txt, "__volatile") || streq_lit(txt, "__volatile__") || streq_lit(txt, "_Noreturn") ||
                       streq_lit(txt, "LLVM_C_ABI") || streq_lit(txt, "LLVM_C_EXTERN_C_BEGIN") ||
                       streq_lit(txt, "LLVM_C_EXTERN_C_END") || streq_lit(txt, "LLVM_ATTRIBUTE_C_DEPRECATED")) {
                c_eat(p, C_TOKEN_IDENTIFIER);
            } else if (!c_is_type_token(p, p->current) && (c_is_type_token(p, look_tok) || look_tok.type == C_TOKEN_CONST ||
                       look_tok.type == C_TOKEN_VOLATILE || look_tok.type == C_TOKEN_RESTRICT || look_tok.type == C_TOKEN_STAR)) {
                c_eat(p, C_TOKEN_IDENTIFIER);
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

static Parameter* c_parse_parameters(CParser *p, int *out_is_varargs);

static VarType c_parse_c_type(CParser *p, int *out_ptr_depth, int *out_array_size) {
    c_skip_modifiers(p);
    VarType type = {TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    int ptr_depth = 0;
    int array_size = 0;
    int is_unsigned = 0;
    int is_signed = 0;
    int is_short = 0;
    int long_count = 0;

    c_skip_modifiers(p);

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
                if (c_match(p, C_TOKEN_IDENTIFIER)) c_eat(p, C_TOKEN_IDENTIFIER);
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

    if (c_match(p, C_TOKEN_COMPLEX)) {
        c_eat(p, C_TOKEN_COMPLEX);
        return c_parse_c_type(p, out_ptr_depth, out_array_size);
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
    } else if (c_match(p, C_TOKEN_INT) || (is_signed && !is_short && long_count == 0) || (is_unsigned && !is_short && long_count == 0)) {
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
    } else if (c_match(p, C_TOKEN_IDENTIFIER)) {
        VarType typedef_type = c_lookup_typedef(p, p->current.text);
        if (typedef_type.base != TYPE_UNKNOWN) {
            type = typedef_type;
        } else {
            BaseType builtin = c_identifier_to_base_type(p->current.text);
            if (builtin != TYPE_UNKNOWN) {
                type.base = builtin;
            } else {
                CLexer look_l = p->lexer;
                CToken look_tok = c_lexer_next(&look_l);
                if (c_is_type_token(p, look_tok) || look_tok.type == C_TOKEN_CONST ||
                    look_tok.type == C_TOKEN_VOLATILE || look_tok.type == C_TOKEN_RESTRICT || look_tok.type == C_TOKEN_STAR) {
                    c_eat(p, C_TOKEN_IDENTIFIER);
                    return c_parse_c_type(p, out_ptr_depth, out_array_size);
                }
                if (strncmp(p->current.text, "__", 2) == 0) {
                    return (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
                }
                type.base = TYPE_CLASS;
                type.class_name = arena_strdup(p->ctx->arena, p->current.text);
            }
        }
        c_eat(p, C_TOKEN_IDENTIFIER);
        if (c_match(p, C_TOKEN_LT)) {
            c_eat(p, C_TOKEN_LT);
            int depth = 1;
            while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                if (c_match(p, C_TOKEN_LT)) depth++;
                else if (c_match(p, C_TOKEN_GT)) depth--;
                c_eat(p, p->current.type);
            }
        }
    } else {
        return (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
    }

    while (c_match(p, C_TOKEN_STAR) || c_match(p, C_TOKEN_AMPERSAND) || c_match(p, C_TOKEN_AND)) {
        c_eat(p, p->current.type);
        ptr_depth++;
        while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT)) {
            c_eat(p, p->current.type);
        }
        while (c_match(p, C_TOKEN_IDENTIFIER)) {
            if (streq_lit(p->current.text, "__restrict") ||
                streq_lit(p->current.text, "__volatile") ||
                streq_lit(p->current.text, "__const") ||
                streq_lit(p->current.text, "__restrict__")) {
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
                if (c_match(p, C_TOKEN_IDENTIFIER)) c_eat(p, C_TOKEN_IDENTIFIER);
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

    c_skip_modifiers(p);

    int ptr_depth = 0, array_size = 0;
    VarType ret_type = c_parse_c_type(p, &ptr_depth, &array_size);
    if (ret_type.base == TYPE_UNKNOWN) {
        ret_type = (VarType){TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
        p->has_error = 0;
    }

    c_skip_modifiers(p);

    if (c_match(p, C_TOKEN_IDENTIFIER) && streq_lit(p->current.text, "as")) {
        c_eat(p, C_TOKEN_IDENTIFIER);
        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            extern_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);
        }
    }

    char *func_name = NULL;
    int in_macro_wrapper = 0;

    c_skip_modifiers(p);

    if (c_match(p, C_TOKEN_IDENTIFIER)) {
        const char *w_name = p->current.text;
        debug_c_header("w_name=%s\n", w_name);
        if (streq_lit(w_name, "__NTH") || streq_lit(w_name, "__NTHNL") ||
            strncmp(w_name, "__REDIRECT", 10) == 0 || strncmp(w_name, "__LDBL_REDIR", 12) == 0) {
            CLexer look_l = p->lexer;
            CToken tok1 = c_lexer_next(&look_l);
            debug_c_header("__NTH tok1.type=%d\n", tok1.type);
            if (tok1.type == C_TOKEN_LPAREN) {
                CToken tok2 = c_lexer_next(&look_l);
                if (tok2.type == C_TOKEN_IDENTIFIER) {
                    CToken tok3 = c_lexer_next(&look_l);
                    if (tok3.type == C_TOKEN_LPAREN || tok3.type == C_TOKEN_COMMA || tok3.type == C_TOKEN_RPAREN) {
                        in_macro_wrapper = 1;
                        c_eat(p, C_TOKEN_IDENTIFIER);
                        c_eat(p, C_TOKEN_LPAREN);
                        func_name = arena_strdup(p->ctx->arena, p->current.text);
                        c_eat(p, C_TOKEN_IDENTIFIER);
                        if (c_match(p, C_TOKEN_COMMA)) {
                            c_eat(p, C_TOKEN_COMMA);
                        }
                    }
                }
            }
        }
    }

    if (!func_name && c_match(p, C_TOKEN_IDENTIFIER)) {
        func_name = arena_strdup(p->ctx->arena, p->current.text);
        c_eat(p, C_TOKEN_IDENTIFIER);
    }

    if (!func_name) {
        c_parser_error(p, "Expected function name in extern declaration");
        return NULL;
    }

    Parameter *params = NULL;
    int is_varargs = 0;

    if (c_match(p, C_TOKEN_LPAREN)) {
        c_eat(p, C_TOKEN_LPAREN);
        if (!c_match(p, C_TOKEN_RPAREN)) {
            params = c_parse_parameters(p, &is_varargs);
        }
        c_eat(p, C_TOKEN_RPAREN);
    }
    if (func_name) {
        int param_count = 0;
        Parameter *curr = params;
        while (curr) { param_count++; curr = curr->next; }
        debug_c_header("parsed function %s with %d params (varargs=%d) at line %d\n", func_name, param_count, is_varargs, p->current.line);
    }

    if (in_macro_wrapper) {
        while (!c_match(p, C_TOKEN_RPAREN) && !c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_LBRACE) && !c_match(p, C_TOKEN_EOF) && !p->has_error) {
            c_eat(p, p->current.type);
        }
        if (c_match(p, C_TOKEN_RPAREN)) {
            c_eat(p, C_TOKEN_RPAREN);
        }
    }

    c_skip_modifiers(p);

    if (c_match(p, C_TOKEN_LBRACE)) {
        c_eat(p, C_TOKEN_LBRACE);
        int depth = 1;
        while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
            if (c_match(p, C_TOKEN_LBRACE)) depth++;
            else if (c_match(p, C_TOKEN_RBRACE)) depth--;
            c_eat(p, p->current.type);
        }
    } else if (c_match(p, C_TOKEN_SEMICOLON)) {
        c_eat(p, C_TOKEN_SEMICOLON);
    }

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
    func->is_varargs = is_varargs;

    return (ASTNode*)func;
}

static Parameter* c_parse_parameters(CParser *p, int *out_is_varargs) {
    Parameter *head = NULL;
    Parameter **curr = &head;
    if (out_is_varargs) *out_is_varargs = 0;

    int extra_paren = 0;
    if (c_match(p, C_TOKEN_LPAREN)) {
        c_eat(p, C_TOKEN_LPAREN);
        extra_paren = 1;
    }

    while (!c_match(p, C_TOKEN_RPAREN) && !p->has_error) {
        if (c_match(p, C_TOKEN_ELLIPSIS)) {
            if (out_is_varargs) *out_is_varargs = 1;
            c_eat(p, C_TOKEN_ELLIPSIS);
            break;
        }

        c_skip_modifiers(p);

        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            if (strncmp(p->current.text, "__fortify_clang_overload_arg", 28) == 0) {
                c_eat(p, C_TOKEN_IDENTIFIER);
                c_eat(p, C_TOKEN_LPAREN);
                int pd = 0, as = 0;
                VarType p_type = c_parse_c_type(p, &pd, &as);
                if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);
                c_skip_modifiers(p);
                if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);
                char *p_name = NULL;
                if (c_match(p, C_TOKEN_IDENTIFIER)) {
                    p_name = arena_strdup(p->ctx->arena, p->current.text);
                    c_eat(p, C_TOKEN_IDENTIFIER);
                }
                while (!c_match(p, C_TOKEN_RPAREN) && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                    c_eat(p, p->current.type);
                }
                if (c_match(p, C_TOKEN_RPAREN)) c_eat(p, C_TOKEN_RPAREN);
                if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);

                Parameter *param = arena_alloc(p->ctx->arena, sizeof(Parameter));
                memset(param, 0, sizeof(Parameter));
                p_type.ptr_depth += pd;
                if (as > 0) p_type.array_size = as;
                param->type = p_type;
                param->name = p_name;

                *curr = param;
                curr = &param->next;
                continue;
            }

            CLexer look_l = p->lexer;
            CToken tok1 = c_lexer_next(&look_l);
            if (tok1.type == C_TOKEN_LPAREN) {
                CToken tok2 = c_lexer_next(&look_l);
                if (tok2.type != C_TOKEN_STAR) {
                    c_eat(p, C_TOKEN_IDENTIFIER);
                    c_eat(p, C_TOKEN_LPAREN);
                    int depth = 1;
                    while (depth > 0 && !p->has_error && p->current.type != C_TOKEN_EOF) {
                        if (c_match(p, C_TOKEN_LPAREN)) depth++;
                        else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                        c_eat(p, p->current.type);
                    }
                    if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);
                    continue;
                }
            }
        }

        int ptr_depth = 0;
        int array_size = 0;
        VarType param_type = c_parse_c_type(p, &ptr_depth, &array_size);

        char *param_name = NULL;
        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            param_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);
        }

        if (param_type.base == TYPE_VOID && ptr_depth == 0 && array_size == 0 && head == NULL) {
            if (c_match(p, C_TOKEN_RPAREN)) {
                break;
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

        if (c_match(p, C_TOKEN_ASSIGN)) {
            c_eat(p, C_TOKEN_ASSIGN);
            while (!c_match(p, C_TOKEN_COMMA) && !c_match(p, C_TOKEN_RPAREN) && !p->has_error) {
                c_eat(p, p->current.type);
            }
        }

        Parameter *param = arena_alloc(p->ctx->arena, sizeof(Parameter));
        memset(param, 0, sizeof(Parameter));
        param_type.ptr_depth += ptr_depth;
        if (array_size > 0) param_type.array_size = array_size;
        param->type = param_type;
        param->name = param_name;

        *curr = param;
        curr = &param->next;

        if (c_match(p, C_TOKEN_COMMA)) {
            c_eat(p, C_TOKEN_COMMA);
            if (c_match(p, C_TOKEN_ELLIPSIS)) {
                if (out_is_varargs) *out_is_varargs = 1;
                c_eat(p, C_TOKEN_ELLIPSIS);
                break;
            }
        } else {
            break;
        }
    }

    if (extra_paren && c_match(p, C_TOKEN_RPAREN)) {
        c_eat(p, C_TOKEN_RPAREN);
    }

    return head;
}

static ASTNode* c_parse_struct_or_union(CParser *p, int is_union) {
    if (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION)) {
        c_eat(p, p->current.type);
    } else if (c_match(p, C_TOKEN_IDENTIFIER) && (streq_lit(p->current.text, "class") || streq_lit(p->current.text, "struct") || streq_lit(p->current.text, "union"))) {
        c_eat(p, C_TOKEN_IDENTIFIER);
    }

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

    char *parent_name = NULL;
    if (c_match(p, C_TOKEN_COLON)) {
        c_eat(p, C_TOKEN_COLON);
        while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) ||
               (c_match(p, C_TOKEN_IDENTIFIER) && (streq_lit(p->current.text, "public") || streq_lit(p->current.text, "protected") || streq_lit(p->current.text, "private") || streq_lit(p->current.text, "virtual")))) {
            c_eat(p, p->current.type);
        }
        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            parent_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);
            if (c_match(p, C_TOKEN_LT)) {
                c_eat(p, C_TOKEN_LT);
                int depth = 1;
                while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                    if (c_match(p, C_TOKEN_LT)) depth++;
                    else if (c_match(p, C_TOKEN_GT)) depth--;
                    c_eat(p, p->current.type);
                }
            }
        }
        while (c_match(p, C_TOKEN_COMMA)) {
            c_eat(p, C_TOKEN_COMMA);
            while (!c_match(p, C_TOKEN_LBRACE) && !c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_COMMA) && !p->has_error) {
                c_eat(p, p->current.type);
            }
        }
    }

    if (!c_match(p, C_TOKEN_LBRACE)) {
        c_eat(p, C_TOKEN_SEMICOLON);
        StructNode *sn = arena_alloc(p->ctx->arena, sizeof(StructNode));
        memset(sn, 0, sizeof(StructNode));
        sn->base.type = NODE_STRUCT;
        sn->base.line = p->current.line;
        sn->base.col = p->current.col;
        sn->name = name;
        sn->parent_name = parent_name;
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
    sn->parent_name = parent_name;
    sn->is_union = is_union;
    sn->has_body = 1;
    sn->is_extern = 1;

    ASTNode **curr_member = &sn->members;

            while (!c_match(p, C_TOKEN_RBRACE) && !p->has_error) { debug_c_header("LOOP START token=%d line=%d text='%s'\n", p->current.type, p->current.line, p->current.text ? p->current.text : "N/A");
        if (c_match(p, C_TOKEN_IDENTIFIER) &&
            (streq_lit(p->current.text, "public") || streq_lit(p->current.text, "protected") || streq_lit(p->current.text, "private"))) {
            c_eat(p, C_TOKEN_IDENTIFIER);
            if (c_match(p, C_TOKEN_COLON)) {
                c_eat(p, C_TOKEN_COLON);
                continue;
            }
        }

        while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT) || c_match(p, C_TOKEN_EXTENSION) || c_match(p, C_TOKEN_STATIC) || c_match(p, C_TOKEN_INLINE) ||
               (c_match(p, C_TOKEN_IDENTIFIER) && (streq_lit(p->current.text, "static") || streq_lit(p->current.text, "inline") || streq_lit(p->current.text, "virtual") || streq_lit(p->current.text, "explicit") || streq_lit(p->current.text, "constexpr") || streq_lit(p->current.text, "consteval")))) {
            c_eat(p, p->current.type);
        }

        if (c_match(p, C_TOKEN_TILDE)) {
            c_eat(p, C_TOKEN_TILDE);
            if (c_match(p, C_TOKEN_IDENTIFIER)) c_eat(p, C_TOKEN_IDENTIFIER);
            if (c_match(p, C_TOKEN_LPAREN)) {
                c_eat(p, C_TOKEN_LPAREN);
                int depth = 1;
                while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                    if (c_match(p, C_TOKEN_LPAREN)) depth++;
                    else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                    c_eat(p, p->current.type);
                }
            }
            if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
            continue;
        }

        if (name && c_match(p, C_TOKEN_IDENTIFIER) && streq(p->current.text, name)) {
            CLexer save_l = p->lexer; CToken save_c = p->current; int save_e = p->has_error;
            c_eat(p, C_TOKEN_IDENTIFIER);
            if (c_match(p, C_TOKEN_LPAREN)) {
                c_eat(p, C_TOKEN_LPAREN);
                int depth = 1;
                while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                    if (c_match(p, C_TOKEN_LPAREN)) depth++;
                    else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                    c_eat(p, p->current.type);
                }
                while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) ||
                       (c_match(p, C_TOKEN_IDENTIFIER) && (streq_lit(p->current.text, "noexcept") || streq_lit(p->current.text, "override")))) {
                    c_eat(p, p->current.type);
                }
                if (c_match(p, C_TOKEN_COLON)) {
                    c_eat(p, C_TOKEN_COLON);
                    while (!c_match(p, C_TOKEN_LBRACE) && !c_match(p, C_TOKEN_SEMICOLON) && !p->has_error) {
                        c_eat(p, p->current.type);
                    }
                }
                if (c_match(p, C_TOKEN_LBRACE)) {
                    c_eat(p, C_TOKEN_LBRACE);
                    int depth = 1;
                    while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                        if (c_match(p, C_TOKEN_LBRACE)) depth++;
                        else if (c_match(p, C_TOKEN_RBRACE)) depth--;
                        c_eat(p, p->current.type);
                    }
                } else if (c_match(p, C_TOKEN_SEMICOLON)) {
                    c_eat(p, C_TOKEN_SEMICOLON);
                }
                continue;
            }
            p->lexer = save_l; p->current = save_c; p->has_error = save_e;
        }

        int ptr_depth = 0;
        int array_size = 0;
        VarType member_type = c_parse_c_type(p, &ptr_depth, &array_size);

        if (member_type.base == TYPE_UNKNOWN) {
            debug_c_header("Unknown type token type=%d text='%s' at line:%d\n", p->current.type, p->current.text ? p->current.text : "N/A", p->current.line); c_parser_error(p, "Unknown type in struct/union member");
            while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_RBRACE) && !p->has_error) {
                c_eat(p, p->current.type);
            }
            if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
            continue;
        }

        if (member_type.base == TYPE_CLASS && c_match(p, C_TOKEN_LBRACE)) {
            c_eat(p, C_TOKEN_LBRACE);
            StructNode *sn = arena_alloc(p->ctx->arena, sizeof(StructNode));
            memset(sn, 0, sizeof(StructNode));
            sn->base.type = NODE_STRUCT;
            sn->base.line = p->current.line;
            sn->base.col = p->current.col;
            sn->name = arena_strdup(p->ctx->arena, member_type.class_name ? member_type.class_name : "__anonymous_struct");
            sn->is_union = 0;
            sn->has_body = 1;
            sn->is_extern = 1;

            ASTNode **curr_member = &sn->members;
    while (!c_match(p, C_TOKEN_RBRACE) && !p->has_error) { debug_c_header("LOOP START token=%d line=%d text='%s'\n", p->current.type, p->current.line, p->current.text ? p->current.text : "N/A");
                while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT) || c_match(p, C_TOKEN_EXTENSION)) {
                    c_eat(p, p->current.type);
                }

                int inner_ptr = 0;
                int inner_array = 0;
                VarType inner_type = c_parse_c_type(p, &inner_ptr, &inner_array);

                if (inner_type.base == TYPE_UNKNOWN) {
                    debug_c_header("Unknown type token type=%d text='%s' at line:%d\n", p->current.type, p->current.text ? p->current.text : "N/A", p->current.line); c_parser_error(p, "Unknown type in struct/union member");
                    while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_RBRACE) && !p->has_error) {
                        c_eat(p, p->current.type);
                    }
                    if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                    continue;
                }

                if (inner_type.base == TYPE_CLASS && c_match(p, C_TOKEN_LBRACE)) {
                    c_eat(p, C_TOKEN_LBRACE);
                    StructNode *anon = arena_alloc(p->ctx->arena, sizeof(StructNode));
                    memset(anon, 0, sizeof(StructNode));
                    anon->base.type = NODE_STRUCT;
                    anon->base.line = p->current.line;
                    anon->base.col = p->current.col;
                    anon->name = arena_strdup(p->ctx->arena, inner_type.class_name ? inner_type.class_name : "__anonymous_struct");
                    anon->is_union = 0;
                    anon->has_body = 1;
                    anon->is_extern = 1;
                    ASTNode **inner_curr = &anon->members;
                    while (!c_match(p, C_TOKEN_RBRACE) && !p->has_error) { debug_c_header("LOOP START token=%d\n", p->current.type);
                        while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT) || c_match(p, C_TOKEN_EXTENSION)) {
                            c_eat(p, p->current.type);
                        }
                        int ip = 0, ia = 0;
                        VarType it = c_parse_c_type(p, &ip, &ia);
                        if (it.base == TYPE_UNKNOWN) {
                            debug_c_header("Unknown type token type=%d text='%s' at line:%d\n", p->current.type, p->current.text ? p->current.text : "N/A", p->current.line); c_parser_error(p, "Unknown type in struct/union member");
                            while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_RBRACE) && !p->has_error) {
                                c_eat(p, p->current.type);
                            }
                            if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                            continue;
                        }
                        if (c_match(p, C_TOKEN_IDENTIFIER)) {
                            VarDeclNode *v = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
                            memset(v, 0, sizeof(VarDeclNode));
                            v->base.type = NODE_VAR_DECL;
                            v->base.line = p->current.line;
                            v->base.col = p->current.col;
                            v->name = arena_strdup(p->ctx->arena, p->current.text);
                            c_eat(p, C_TOKEN_IDENTIFIER);
                            v->var_type = it;
                            v->var_type.ptr_depth += ip;
                            v->var_type.array_size = ia > 0 ? ia : it.array_size;
                            v->is_array = ia > 0;
                            v->is_mutable = 1;
                            *inner_curr = (ASTNode*)v;
                            inner_curr = &v->base.next;
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
                        if (c_match(p, C_TOKEN_COLON)) {
                            c_eat(p, C_TOKEN_COLON);
                            if (c_match(p, C_TOKEN_NUMBER)) c_eat(p, C_TOKEN_NUMBER);
                        }
                        if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                    }
                    c_eat(p, C_TOKEN_RBRACE);
                    *curr_member = (ASTNode*)anon;
                    curr_member = &anon->base.next;
                    if (c_match(p, C_TOKEN_SEMICOLON)) {
                        c_eat(p, C_TOKEN_SEMICOLON);
                    } else if (c_match(p, C_TOKEN_COMMA)) {
                        while (c_match(p, C_TOKEN_COMMA)) {
                            c_eat(p, C_TOKEN_COMMA);
                            if (c_match(p, C_TOKEN_IDENTIFIER)) c_eat(p, C_TOKEN_IDENTIFIER);
                        }
                        if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                    }
                    continue;
                }

                if (c_match(p, C_TOKEN_IDENTIFIER)) {
                    VarDeclNode *var = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
                    memset(var, 0, sizeof(VarDeclNode));
                    var->base.type = NODE_VAR_DECL;
                    var->base.line = p->current.line;
                    var->base.col = p->current.col;
                    var->name = arena_strdup(p->ctx->arena, p->current.text);
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
                    while (c_match(p, C_TOKEN_IDENTIFIER)) {
                        c_eat(p, C_TOKEN_IDENTIFIER);
                    }
                    if (c_match(p, C_TOKEN_COLON)) {
                        c_eat(p, C_TOKEN_COLON);
                        if (c_match(p, C_TOKEN_NUMBER)) c_eat(p, C_TOKEN_NUMBER);
                    }
                    var->var_type = inner_type;
                    var->var_type.ptr_depth += inner_ptr;
                    var->var_type.array_size = inner_array > 0 ? inner_array : inner_type.array_size;
                    var->is_array = inner_array > 0;
                    var->is_mutable = 1;
                    *curr_member = (ASTNode*)var;
                    curr_member = &var->base.next;
                }

                if (c_match(p, C_TOKEN_SEMICOLON)) {
                    c_eat(p, C_TOKEN_SEMICOLON);
                } else if (c_match(p, C_TOKEN_COMMA)) {
                    c_eat(p, C_TOKEN_COMMA);
                    while (c_match(p, C_TOKEN_IDENTIFIER)) {
                        VarDeclNode *var = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
                        memset(var, 0, sizeof(VarDeclNode));
                        var->base.type = NODE_VAR_DECL;
                        var->base.line = p->current.line;
                        var->base.col = p->current.col;
                        var->name = arena_strdup(p->ctx->arena, p->current.text);
                        c_eat(p, C_TOKEN_IDENTIFIER);
                        var->var_type = inner_type;
                        var->var_type.ptr_depth += inner_ptr;
                        var->var_type.array_size = inner_array > 0 ? inner_array : inner_type.array_size;
                        var->is_array = inner_array > 0;
                        var->is_mutable = 1;
                        *curr_member = (ASTNode*)var;
                        curr_member = &var->base.next;
                        if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);
                        else break;
                    }
                    if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                } else {
                    while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_RBRACE) && !c_match(p, C_TOKEN_COMMA) && !p->has_error) {
                        c_eat(p, p->current.type);
                    }
                    if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                    else if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);
                }
            }
            c_eat(p, C_TOKEN_RBRACE);
            if (c_match(p, C_TOKEN_IDENTIFIER)) {
                char *anon_name = arena_strdup(p->ctx->arena, p->current.text);
                c_eat(p, C_TOKEN_IDENTIFIER);
                VarDeclNode *var = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
                memset(var, 0, sizeof(VarDeclNode));
                var->base.type = NODE_VAR_DECL;
                var->base.line = sn->base.line;
                var->base.col = sn->base.col;
                var->name = anon_name;
                var->var_type.base = TYPE_CLASS;
                var->var_type.class_name = arena_strdup(p->ctx->arena, sn->name);
                var->var_type.ptr_depth = 0;
                var->is_mutable = 1;
                *curr_member = (ASTNode*)var;
                curr_member = &var->base.next;
            }
            *curr_member = (ASTNode*)sn;
            curr_member = &sn->base.next;
            if (c_match(p, C_TOKEN_SEMICOLON)) {
                c_eat(p, C_TOKEN_SEMICOLON);
            } else if (c_match(p, C_TOKEN_COMMA)) {
                while (c_match(p, C_TOKEN_COMMA)) {
                    c_eat(p, C_TOKEN_COMMA);
                    if (c_match(p, C_TOKEN_IDENTIFIER)) {
                        char *anon_name2 = arena_strdup(p->ctx->arena, p->current.text);
                        c_eat(p, C_TOKEN_IDENTIFIER);
                        VarDeclNode *var2 = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
                        memset(var2, 0, sizeof(VarDeclNode));
                        var2->base.type = NODE_VAR_DECL;
                        var2->base.line = sn->base.line;
                        var2->base.col = sn->base.col;
                        var2->name = anon_name2;
                        var2->var_type.base = TYPE_CLASS;
                        var2->var_type.class_name = arena_strdup(p->ctx->arena, sn->name);
                        var2->var_type.ptr_depth = 0;
                        var2->is_mutable = 1;
                        *curr_member = (ASTNode*)var2;
                        curr_member = &var2->base.next;
                    }
                }
                if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
            }
            continue;
        }

        if (c_match(p, C_TOKEN_IDENTIFIER)) {
            char *member_name = arena_strdup(p->ctx->arena, p->current.text);
            c_eat(p, C_TOKEN_IDENTIFIER);

            if (c_match(p, C_TOKEN_LPAREN)) {
                c_eat(p, C_TOKEN_LPAREN);
                int depth = 1;
                while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                    if (c_match(p, C_TOKEN_LPAREN)) depth++;
                    else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                    c_eat(p, p->current.type);
                }
                while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_LBRACE) && !p->has_error) {
                    c_eat(p, p->current.type);
                }
                if (c_match(p, C_TOKEN_LBRACE)) {
                    c_eat(p, C_TOKEN_LBRACE);
                    int depth = 1;
                    while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                        if (c_match(p, C_TOKEN_LBRACE)) depth++;
                        else if (c_match(p, C_TOKEN_RBRACE)) depth--;
                        c_eat(p, p->current.type);
                    }
                } else if (c_match(p, C_TOKEN_SEMICOLON)) {
                    c_eat(p, C_TOKEN_SEMICOLON);
                }
                continue;
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
            while (c_match(p, C_TOKEN_IDENTIFIER)) {
                c_eat(p, C_TOKEN_IDENTIFIER);
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

        if (c_match(p, C_TOKEN_SEMICOLON)) {
            c_eat(p, C_TOKEN_SEMICOLON);
        } else if (c_match(p, C_TOKEN_COMMA)) {
            while (c_match(p, C_TOKEN_COMMA)) {
                c_eat(p, C_TOKEN_COMMA);
                int extra_ptr = 0;
                while (c_match(p, C_TOKEN_STAR)) {
                    extra_ptr++;
                    c_eat(p, C_TOKEN_STAR);
                }
                if (c_match(p, C_TOKEN_IDENTIFIER)) {
                    char *mname = arena_strdup(p->ctx->arena, p->current.text);
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
                    
                    while (c_match(p, C_TOKEN_IDENTIFIER)) {
                        c_eat(p, C_TOKEN_IDENTIFIER);
                    }
                    
                    VarDeclNode *v = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
                    memset(v, 0, sizeof(VarDeclNode));
                    v->base.type = NODE_VAR_DECL;
                    v->base.line = p->current.line;
                    v->base.col = p->current.col;
                    v->name = mname;
                    v->var_type = member_type;
                    v->var_type.ptr_depth += ptr_depth + extra_ptr;
                    v->var_type.array_size = array_size > 0 ? array_size : member_type.array_size;
                    v->is_array = array_size > 0;
                    v->is_mutable = 1;
                    
                    *curr_member = (ASTNode*)v;
                    curr_member = &v->base.next;
                }
            }
            if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
        }
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

    if (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION) ||
        (c_match(p, C_TOKEN_IDENTIFIER) && (streq_lit(p->current.text, "class") || streq_lit(p->current.text, "struct")))) {
        c_eat(p, p->current.type);
    }

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

    if (c_match(p, C_TOKEN_COLON)) {
        c_eat(p, C_TOKEN_COLON);
        while (!c_match(p, C_TOKEN_LBRACE) && !c_match(p, C_TOKEN_SEMICOLON) && !p->has_error) {
            c_eat(p, p->current.type);
        }
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

    while (!c_match(p, C_TOKEN_RBRACE) && !p->has_error) { debug_c_header("LOOP START token=%d\n", p->current.type);
        if (!c_match(p, C_TOKEN_IDENTIFIER)) {
            c_parser_error(p, "Expected enumerator name");
            break;
        }

        char *entry_name = arena_strdup(p->ctx->arena, p->current.text);
        c_eat(p, C_TOKEN_IDENTIFIER);

        if (c_match(p, C_TOKEN_LPAREN)) {
            c_eat(p, C_TOKEN_LPAREN);
            int depth = 1;
            while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                if (c_match(p, C_TOKEN_LPAREN)) depth++;
                else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                c_eat(p, p->current.type);
            }
            if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);
            continue;
        }

        if (c_match(p, C_TOKEN_ASSIGN)) {
            c_eat(p, C_TOKEN_ASSIGN);
            if (c_match(p, C_TOKEN_NUMBER)) {
                value = (int)p->current.int_val;
                c_eat(p, C_TOKEN_NUMBER);
            } else if (c_match(p, C_TOKEN_IDENTIFIER)) {
                c_eat(p, C_TOKEN_IDENTIFIER);
            }
            while (!c_match(p, C_TOKEN_COMMA) && !c_match(p, C_TOKEN_RBRACE) && !p->has_error) {
                if (c_match(p, C_TOKEN_LPAREN)) {
                    c_eat(p, C_TOKEN_LPAREN);
                    int depth = 1;
                    while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                        if (c_match(p, C_TOKEN_LPAREN)) depth++;
                        else if (c_match(p, C_TOKEN_RPAREN)) depth--;
                        c_eat(p, p->current.type);
                    }
                } else {
                    c_eat(p, p->current.type);
                }
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

    if (c_match(p, C_TOKEN_IDENTIFIER)) {
        char *typedef_name = arena_strdup(p->ctx->arena, p->current.text);
        c_eat(p, C_TOKEN_IDENTIFIER);
        if (en->name && strncmp(en->name, "__anon_enum", 11) == 0) {
            en->name = typedef_name;
        }
        VarType t;
        memset(&t, 0, sizeof(VarType));
        t.base = TYPE_ENUM;
        t.class_name = en->name;
        c_register_typedef(p, typedef_name, t);
    }

    if (c_match(p, C_TOKEN_SEMICOLON)) {
        c_eat(p, C_TOKEN_SEMICOLON);
    }

    return (ASTNode*)en;
}

static ASTNode* c_parse_typedef(CParser *p) {
    c_eat(p, C_TOKEN_TYPEDEF);

    CLexer look_l = p->lexer;
    CToken t2 = c_lexer_next(&look_l);
    int is_standalone = 0;
    
    if (c_match(p, C_TOKEN_ENUM) || c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION)) {
        if (t2.type == C_TOKEN_LBRACE || t2.type == C_TOKEN_COLON || t2.type == C_TOKEN_ATTRIBUTE) {
            is_standalone = 1;
        } else if (t2.type == C_TOKEN_IDENTIFIER) {
            CToken t3 = c_lexer_next(&look_l);
            if (t3.type == C_TOKEN_LBRACE || t3.type == C_TOKEN_SEMICOLON || t3.type == C_TOKEN_COLON || t3.type == C_TOKEN_ATTRIBUTE) {
                is_standalone = 1;
            }
        }
    }
    
    if (is_standalone) {
        if (c_match(p, C_TOKEN_ENUM)) {
            return c_parse_enum(p);
        }
    }
    
    if (!is_standalone && c_match(p, C_TOKEN_ENUM)) {
        // Not standalone enum, let it fall through to c_parse_c_type
    } else if (is_standalone && (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION))) {
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

            while (!c_match(p, C_TOKEN_RBRACE) && !p->has_error) { debug_c_header("LOOP START token=%d\n", p->current.type);
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
                        int depth = 1;
                        while (depth > 0 && !p->has_error) {
                            if (c_match(p, C_TOKEN_LBRACKET)) { depth++; c_eat(p, C_TOKEN_LBRACKET); }
                            else if (c_match(p, C_TOKEN_RBRACKET)) { depth--; c_eat(p, C_TOKEN_RBRACKET); }
                            else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                            else { c_eat(p, p->current.type); }
                        }
                    }
                    while (c_match(p, C_TOKEN_IDENTIFIER)) {
                        c_eat(p, C_TOKEN_IDENTIFIER);
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
                } else if (member_type.base == TYPE_CLASS && c_match(p, C_TOKEN_LBRACE)) {
                    c_eat(p, C_TOKEN_LBRACE);
                    StructNode *anon = arena_alloc(p->ctx->arena, sizeof(StructNode));
                    memset(anon, 0, sizeof(StructNode));
                    anon->base.type = NODE_STRUCT;
                    anon->base.line = p->current.line;
                    anon->base.col = p->current.col;
                    anon->name = arena_strdup(p->ctx->arena, member_type.class_name ? member_type.class_name : "__anonymous_struct");
                    anon->is_union = 0;
                    anon->has_body = 1;
                    anon->is_extern = 1;
                    ASTNode **inner_curr = &anon->members;
                    while (!c_match(p, C_TOKEN_RBRACE) && !p->has_error) { debug_c_header("LOOP START token=%d\n", p->current.type);
                        while (c_match(p, C_TOKEN_CONST) || c_match(p, C_TOKEN_VOLATILE) || c_match(p, C_TOKEN_RESTRICT) || c_match(p, C_TOKEN_EXTENSION)) {
                            c_eat(p, p->current.type);
                        }
                        int ip = 0, ia = 0;
                        VarType it = c_parse_c_type(p, &ip, &ia);
                        if (it.base == TYPE_UNKNOWN) {
                            debug_c_header("Unknown type token type=%d text='%s' at line:%d\n", p->current.type, p->current.text ? p->current.text : "N/A", p->current.line); c_parser_error(p, "Unknown type in struct/union member");
                            while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_RBRACE) && !p->has_error) {
                                c_eat(p, p->current.type);
                            }
                            if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                            continue;
                        }
                        if (c_match(p, C_TOKEN_IDENTIFIER)) {
                            VarDeclNode *v = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
                            memset(v, 0, sizeof(VarDeclNode));
                            v->base.type = NODE_VAR_DECL;
                            v->base.line = p->current.line;
                            v->base.col = p->current.col;
                            v->name = arena_strdup(p->ctx->arena, p->current.text);
                            c_eat(p, C_TOKEN_IDENTIFIER);
                            v->var_type = it;
                            v->var_type.ptr_depth += ip;
                            v->var_type.array_size = ia > 0 ? ia : it.array_size;
                            v->is_array = ia > 0;
                            v->is_mutable = 1;
                            *inner_curr = (ASTNode*)v;
                            inner_curr = &v->base.next;
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
                        if (c_match(p, C_TOKEN_COLON)) {
                            c_eat(p, C_TOKEN_COLON);
                            if (c_match(p, C_TOKEN_NUMBER)) c_eat(p, C_TOKEN_NUMBER);
                        }
                        if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                    }
                    c_eat(p, C_TOKEN_RBRACE);
                    if (c_match(p, C_TOKEN_IDENTIFIER)) {
                        char *anon_name = arena_strdup(p->ctx->arena, p->current.text);
                        c_eat(p, C_TOKEN_IDENTIFIER);
                        VarDeclNode *var = arena_alloc(p->ctx->arena, sizeof(VarDeclNode));
                        memset(var, 0, sizeof(VarDeclNode));
                        var->base.type = NODE_VAR_DECL;
                        var->base.line = anon->base.line;
                        var->base.col = anon->base.col;
                        var->name = anon_name;
                        var->var_type.base = TYPE_CLASS;
                        var->var_type.class_name = arena_strdup(p->ctx->arena, anon->name);
                        var->var_type.ptr_depth = 0;
                        var->is_mutable = 1;
                        *curr_member = (ASTNode*)var;
                        curr_member = &var->base.next;
                    }
                    *curr_member = (ASTNode*)anon;
                    curr_member = &anon->base.next;
                }

                if (c_match(p, C_TOKEN_SEMICOLON)) {
                    c_eat(p, C_TOKEN_SEMICOLON);
                } else {
                    while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_RBRACE) && !c_match(p, C_TOKEN_COMMA) && !p->has_error) {
                        c_eat(p, p->current.type);
                    }
                    if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                    else if (c_match(p, C_TOKEN_COMMA)) c_eat(p, C_TOKEN_COMMA);
                }
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
                    if (streq_lit(p->current.text, "__restrict") ||
                        streq_lit(p->current.text, "__volatile") ||
                        streq_lit(p->current.text, "__const") ||
                        streq_lit(p->current.text, "__restrict__")) {
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
                int fp_is_varargs = 0;
                if (c_match(p, C_TOKEN_LPAREN)) {
                    c_eat(p, C_TOKEN_LPAREN);
                    if (!c_match(p, C_TOKEN_RPAREN)) {
                        params = c_parse_parameters(p, &fp_is_varargs);
                    }
                    c_eat(p, C_TOKEN_RPAREN);
                }

                VarType fp_type;
                memset(&fp_type, 0, sizeof(VarType));
                fp_type.base = TYPE_VOID;
                fp_type.is_func_ptr = 1;
                fp_type.fp_is_varargs = fp_is_varargs;
                fp_type.fp_ret_type = arena_alloc(p->ctx->arena, sizeof(VarType));
                *fp_type.fp_ret_type = base_type;

                int p_count = 0;
                Parameter *p_curr = params;
                while (p_curr) { p_count++; p_curr = p_curr->next; }
                fp_type.fp_param_count = p_count;
                if (p_count > 0) {
                    fp_type.fp_param_types = arena_alloc(p->ctx->arena, sizeof(VarType) * p_count);
                    p_curr = params;
                    for (int i = 0; i < p_count; i++) {
                        fp_type.fp_param_types[i] = p_curr->type;
                        p_curr = p_curr->next;
                    }
                }

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
            int fp_is_varargs = 0;
            if (c_match(p, C_TOKEN_LPAREN)) {
                c_eat(p, C_TOKEN_LPAREN);
                if (!c_match(p, C_TOKEN_RPAREN)) {
                    params = c_parse_parameters(p, &fp_is_varargs);
                }
                c_eat(p, C_TOKEN_RPAREN);
            }

            VarType fp_type;
            memset(&fp_type, 0, sizeof(VarType));
            fp_type.base = TYPE_VOID;
            fp_type.is_func_ptr = 1;
            fp_type.fp_is_varargs = fp_is_varargs;
            fp_type.fp_ret_type = arena_alloc(p->ctx->arena, sizeof(VarType));
            *fp_type.fp_ret_type = base_type;

            int p_count = 0;
            Parameter *p_curr = params;
            while (p_curr) { p_count++; p_curr = p_curr->next; }
            fp_type.fp_param_count = p_count;
            if (p_count > 0) {
                fp_type.fp_param_types = arena_alloc(p->ctx->arena, sizeof(VarType) * p_count);
                p_curr = params;
                for (int i = 0; i < p_count; i++) {
                    fp_type.fp_param_types[i] = p_curr->type;
                    p_curr = p_curr->next;
                }
            }

            if (fp_name) {
                c_register_typedef(p, fp_name, fp_type);
            }
        }
    }

    c_skip_modifiers(p);
    if (c_match(p, C_TOKEN_SEMICOLON)) {
        c_eat(p, C_TOKEN_SEMICOLON);
    }
    return NULL;
}

static ASTNode* c_parse_extern_variable(CParser *p) {
    c_skip_modifiers(p);

    int ptr_depth = 0;
    int array_size = 0;
    VarType var_type = c_parse_c_type(p, &ptr_depth, &array_size);

    c_skip_modifiers(p);

    if (!c_match(p, C_TOKEN_IDENTIFIER)) {
        while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
            c_eat(p, p->current.type);
        }
        if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
        return NULL;
    }

    char *var_name = arena_strdup(p->ctx->arena, p->current.text);
    c_eat(p, C_TOKEN_IDENTIFIER);

    c_skip_modifiers(p);

    while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
        c_eat(p, p->current.type);
    }
    if (c_match(p, C_TOKEN_SEMICOLON)) {
        c_eat(p, C_TOKEN_SEMICOLON);
    }

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
    var->is_extern = 1;

    return (ASTNode*)var;
}

static ASTNode* c_parse_declaration(CParser *p) {
    p->has_error = 0;
    c_skip_modifiers(p);

    if (c_match(p, C_TOKEN_EOF) || p->current.type == C_TOKEN_EOF) return NULL;
    if (c_match(p, C_TOKEN_RBRACE) || c_match(p, C_TOKEN_SEMICOLON)) {
        c_eat(p, p->current.type);
        return NULL;
    }

    if (c_match(p, C_TOKEN_LBRACE)) {
        c_eat(p, C_TOKEN_LBRACE);
        int depth = 1;
        while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
            if (c_match(p, C_TOKEN_LBRACE)) depth++;
            else if (c_match(p, C_TOKEN_RBRACE)) depth--;
            c_eat(p, p->current.type);
        }
        return NULL;
    }

    if (c_match(p, C_TOKEN_SEMICOLON)) {
        c_eat(p, C_TOKEN_SEMICOLON);
        return NULL;
    }

    if (c_match(p, C_TOKEN_TYPEDEF)) {
        return c_parse_typedef(p);
    }

    if (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION) || c_match(p, C_TOKEN_ENUM) ||
        (c_match(p, C_TOKEN_IDENTIFIER) && (streq_lit(p->current.text, "class") || streq_lit(p->current.text, "struct") || streq_lit(p->current.text, "union")))) {
        
        CLexer look_l = p->lexer;
        CToken t2 = c_lexer_next(&look_l);
        int is_standalone = 0;
        
        if (t2.type == C_TOKEN_LBRACE || t2.type == C_TOKEN_COLON || t2.type == C_TOKEN_ATTRIBUTE) {
            is_standalone = 1;
        } else if (t2.type == C_TOKEN_IDENTIFIER) {
            CToken t3 = c_lexer_next(&look_l);
            if (t3.type == C_TOKEN_LBRACE || t3.type == C_TOKEN_SEMICOLON || t3.type == C_TOKEN_COLON || t3.type == C_TOKEN_ATTRIBUTE) {
                is_standalone = 1;
            }
        }
        
        if (is_standalone) {
            if (c_match(p, C_TOKEN_ENUM)) {
                return c_parse_enum(p);
            } else {
                return c_parse_struct_or_union(p, c_match(p, C_TOKEN_UNION));
            }
        }
    }

    if (c_match(p, C_TOKEN_IDENTIFIER) && streq_lit(p->current.text, "namespace")) {
        c_eat(p, C_TOKEN_IDENTIFIER);
        while (c_match(p, C_TOKEN_IDENTIFIER) || c_match(p, C_TOKEN_COLON)) {
            c_eat(p, p->current.type);
        }
        if (c_match(p, C_TOKEN_LBRACE)) {
            c_eat(p, C_TOKEN_LBRACE);
            ASTNode *head = NULL;
            ASTNode **curr = &head;
            while (!c_match(p, C_TOKEN_RBRACE) && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                ASTNode *decl = c_parse_declaration(p);
                if (decl) {
                    *curr = decl;
                    while (*curr) curr = &(*curr)->next;
                } else if (p->has_error) {
                    while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_RBRACE) && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                        c_eat(p, p->current.type);
                    }
                    if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
                    p->has_error = 0;
                }
            }
            if (c_match(p, C_TOKEN_RBRACE)) c_eat(p, C_TOKEN_RBRACE);
            return head;
        } else if (c_match(p, C_TOKEN_SEMICOLON)) {
            c_eat(p, C_TOKEN_SEMICOLON);
            return NULL;
        }
    }

    if (c_match(p, C_TOKEN_IDENTIFIER) && streq_lit(p->current.text, "using")) {
        c_eat(p, C_TOKEN_IDENTIFIER);
        while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
            c_eat(p, p->current.type);
        }
        if (c_match(p, C_TOKEN_SEMICOLON)) c_eat(p, C_TOKEN_SEMICOLON);
        return NULL;
    }

    if (c_match(p, C_TOKEN_IDENTIFIER) && streq_lit(p->current.text, "template")) {
        c_eat(p, C_TOKEN_IDENTIFIER);
        if (c_match(p, C_TOKEN_LT)) {
            c_eat(p, C_TOKEN_LT);
            int depth = 1;
            while (depth > 0 && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                if (c_match(p, C_TOKEN_LT)) depth++;
                else if (c_match(p, C_TOKEN_GT)) depth--;
                c_eat(p, p->current.type);
            }
        }
        return c_parse_declaration(p);
    }

    if (p->current.type == C_TOKEN_EOF || c_match(p, C_TOKEN_EOF)) {
        return NULL;
    }

    CLexer save_lexer = p->lexer;
    CToken save_current = p->current;
    int save_error = p->has_error;
    (void)save_error;

    p->has_error = 0;
    c_skip_modifiers(p);

    int is_function = 0;
    int pd = 0, as = 0;
    VarType vt = c_parse_c_type(p, &pd, &as);
    (void)vt; (void)pd; (void)as;

    c_skip_modifiers(p);

    if (p->current.type == C_TOKEN_IDENTIFIER) {
        CLexer look_l = p->lexer;
        CToken tok1 = c_lexer_next(&look_l);
        if (tok1.type == C_TOKEN_LPAREN) {
            is_function = 1;
        }
    }

    p->lexer = save_lexer;
    p->current = save_current;
    p->has_error = 0;

    if (is_function) {
        return c_parse_extern_function(p);
    } else {
        return c_parse_extern_variable(p);
    }
}

void c_parser_init(CParser *p, CompilerContext *ctx, const char *filename, const char *source) {
    memset(p, 0, sizeof(CParser));
    c_lexer_init(&p->lexer, ctx, filename, source);
    p->ctx = ctx;
    hashmap_init(&p->typedef_map, ctx ? ctx->arena : NULL, 256);
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
        } else if (p->has_error) {
            while (!c_match(p, C_TOKEN_SEMICOLON) && !c_match(p, C_TOKEN_EOF) && p->current.type != C_TOKEN_EOF) {
                c_eat(p, p->current.type);
            }
            if (c_match(p, C_TOKEN_SEMICOLON)) {
                c_eat(p, C_TOKEN_SEMICOLON);
            }
            p->has_error = 0;
        }
    }

    return head;
}

char* c_preprocess_header(CompilerContext *ctx, const char *fname) {
    char cmd[4096] = {0};
    char include_flags[1024] = {0};

    if (fname[0] == '/') {
        snprintf(cmd, sizeof(cmd), "echo '#include \"%s\"' | gcc -E -DWLR_USE_UNSTABLE -xc - 2>/dev/null", fname);
    } else {
        snprintf(cmd, sizeof(cmd), "echo '#include <%s>' | gcc -E -DWLR_USE_UNSTABLE -I. -xc - 2>/dev/null", fname);
    }

    if (fname[0] == '/') {
        char path_copy[512];
        strncpy(path_copy, fname, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';

        char *dir = path_copy;
        char *last_slash = strrchr(dir, '/');
        if (last_slash) {
            *last_slash = '\0';

            for (int i = 0; i < 4 && dir[0] == '/' && strlen(dir) > 1; i++) {
                char flag[256];
                snprintf(flag, sizeof(flag), " -I%s", dir);
                if (strlen(include_flags) + strlen(flag) + 1 < sizeof(include_flags)) {
                    strcat(include_flags, flag);
                }

                last_slash = strrchr(dir, '/');
                if (last_slash && last_slash != dir) {
                    *last_slash = '\0';
                } else {
                    break;
                }
            }

            char *component = strrchr(fname, '/');
            if (component) {
                component++;
                while (*component) {
                    if ((*component >= 'a' && *component <= 'z') ||
                        (*component >= 'A' && *component <= 'Z') ||
                        (*component >= '0' && *component <= '9') ||
                        *component == '-' || *component == '_' || *component == '.') {
                        component++;
                    } else {
                        break;
                    }
                }
                size_t comp_len = component - (strrchr(fname, '/') + 1);
                if (comp_len > 0 && comp_len < 64) {
                    char pkg_name[64];
                    snprintf(pkg_name, sizeof(pkg_name), "%.*s", (int)comp_len, strrchr(fname, '/') + 1);
                    char pkg_cmd[256];
                    snprintf(pkg_cmd, sizeof(pkg_cmd), "pkg-config --cflags %s 2>/dev/null", pkg_name);
                    FILE *pf = popen(pkg_cmd, "r");
                    if (pf) {
                        char pkg_out[512] = {0};
                        size_t pkg_len = fread(pkg_out, 1, sizeof(pkg_out) - 1, pf);
                        pkg_out[pkg_len] = '\0';
                        pclose(pf);
                        if (pkg_len > 0) {
                            char *p = pkg_out;
                            while (*p) {
                                while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                                if (*p == '-' && *(p+1) == 'I') {
                                    p += 2;
                                    char *end = p;
                                    while (*end && *end != ' ' && *end != '\t' && *end != '\n') end++;
                                    size_t path_len = end - p;
                                    if (path_len > 0 && path_len < 256) {
                                        char flag[256];
                                        snprintf(flag, sizeof(flag), " -I%.*s", (int)path_len, p);
                                        if (strlen(include_flags) + strlen(flag) + 1 < sizeof(include_flags)) {
                                            strcat(include_flags, flag);
                                        }
                                    }
                                    p = end;
                                } else {
                                    while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
                                }
                            }
                        }
                    }
                }
            }
        }

    }

    const char *extra_cflags = getenv("ALKYL_CFLAGS");
    if (extra_cflags) {
        if (strlen(include_flags) + strlen(extra_cflags) + 2 < sizeof(include_flags)) {
            strcat(include_flags, " ");
            strcat(include_flags, extra_cflags);
        }
    }

    if (ctx && ctx->cflags[0]) {
        if (strlen(include_flags) + strlen(ctx->cflags) + 2 < sizeof(include_flags)) {
            strcat(include_flags, " ");
            strcat(include_flags, ctx->cflags);
        }
    }

    if (strlen(include_flags) > 0) {
        char *insert_pos = strstr(cmd, " -xc ");
        if (insert_pos) {
            char new_cmd[4096];
            snprintf(new_cmd, sizeof(new_cmd), "%.*s%s -xc %s", (int)(insert_pos - cmd), cmd, include_flags, insert_pos + 5);
            strncpy(cmd, new_cmd, sizeof(cmd) - 1);
            cmd[sizeof(cmd) - 1] = '\0';
        }
    }

    FILE *f = popen(cmd, "r");
    if (!f) return NULL;

    size_t cap = 16384;
    size_t len = 0;
    char *buf = malloc(cap);

    while (1) {
        size_t bytes = fread(buf + len, 1, cap - len - 1, f);
        if (bytes == 0) break;
        len += bytes;
        if (len >= cap - 1) {
            cap *= 2;
            char *new_buf = realloc(buf, cap);
            buf = new_buf;
        }
    }
    buf[len] = '\0';
    pclose(f);

    if (len == 0) {
        free(buf);
        return NULL;
    }

    char *arena_buf = arena_alloc(ctx->arena, len + 1);
    if (arena_buf) memcpy(arena_buf, buf, len + 1);
    free(buf);

    return arena_buf;
}
