#include "emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StringBuilder;

static void sb_init(StringBuilder *sb) {
    sb->cap = 4096;
    sb->len = 0;
    sb->data = malloc(sb->cap);
    if (sb->data) sb->data[0] = '\0';
}

static void sb_append_fmt(StringBuilder *sb, const char *fmt, ...) {
    if (!sb->data) return;
    va_list args;
    va_start(args, fmt);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (len < 0) { va_end(args); return; }

    if (sb->len + len + 1 >= sb->cap) {
        sb->cap = (sb->cap * 2) + len;
        char *new_data = realloc(sb->data, sb->cap);
        if (!new_data) { free(sb->data); sb->data = NULL; va_end(args); return; }
        sb->data = new_data;
    }
    
    vsnprintf(sb->data + sb->len, len + 1, fmt, args);
    sb->len += len;
    va_end(args);
}

static char* sb_free_and_return(StringBuilder *sb) {
    return sb->data; 
}

// --- Emitter Logic ---

static void emit_indent(StringBuilder *sb, int indent) {
    for (int i = 0; i < indent; i++) sb_append_fmt(sb, "  ");
}

static void emit_type_str(StringBuilder *sb, VarType t) {
    if (t.is_unsigned) sb_append_fmt(sb, "unsigned ");
    
    switch (t.base) {
        case TYPE_INT: sb_append_fmt(sb, "int"); break;
        case TYPE_SHORT: sb_append_fmt(sb, "short"); break;
        case TYPE_LONG: sb_append_fmt(sb, "long"); break;
        case TYPE_LONG_LONG: sb_append_fmt(sb, "long long"); break;
        case TYPE_CHAR: sb_append_fmt(sb, "char"); break;
        case TYPE_BOOL: sb_append_fmt(sb, "bool"); break;
        case TYPE_FLOAT: sb_append_fmt(sb, "single"); break;
        case TYPE_DOUBLE: sb_append_fmt(sb, "double"); break;
        case TYPE_LONG_DOUBLE: sb_append_fmt(sb, "long double"); break;
        case TYPE_VOID: sb_append_fmt(sb, "void"); break;
        case TYPE_STRING: sb_append_fmt(sb, "string"); break;
        case TYPE_AUTO: sb_append_fmt(sb, "let"); break;
        case TYPE_CLASS: sb_append_fmt(sb, "%s", t.class_name ? t.class_name : "class"); break;
        default: sb_append_fmt(sb, "unknown"); break;
    }

    for (int i = 0; i < t.ptr_depth; i++) sb_append_fmt(sb, "*");
    if (t.array_size > 0) sb_append_fmt(sb, "[%d]", t.array_size);
}

static void emit_scope(StringBuilder *sb, SemScope *scope, int indent);

static void emit_symbol(StringBuilder *sb, SemSymbol *sym, int indent) {
    emit_indent(sb, indent);
    
    const char *kind_str = "UNK";
    switch (sym->kind) {
        case SYM_VAR: kind_str = "VAR"; break;
        case SYM_FUNC: kind_str = "FUNC"; break;
        case SYM_CLASS: kind_str = "CLASS"; break;
        case SYM_ENUM: kind_str = "ENUM"; break;
        case SYM_NAMESPACE: kind_str = "NAMESPACE"; break;
    }
    
    sb_append_fmt(sb, "[%s] %s : ", kind_str, sym->name);
    emit_type_str(sb, sym->type);
    
    if (sym->parent_name) {
        sb_append_fmt(sb, " (extends %s)", sym->parent_name);
    }
    
    sb_append_fmt(sb, "\n");

    // Recurse if this symbol has an inner scope (Class, Namespace, Function)
    if (sym->inner_scope) {
        emit_scope(sb, sym->inner_scope, indent + 1);
    }
}

static void emit_scope(StringBuilder *sb, SemScope *scope, int indent) {
    if (!scope) return;
    
    SemSymbol *sym = scope->symbols;
    if (!sym) {
        emit_indent(sb, indent);
        sb_append_fmt(sb, "(empty scope)\n");
        return;
    }

    // Traverse linked list
    while (sym) {
        emit_symbol(sb, sym, indent);
        sym = sym->next;
    }
}

char* semantic_to_string(SemanticCtx *ctx) {
    StringBuilder sb;
    sb_init(&sb);
    if (!sb.data) return NULL;
    
    sb_append_fmt(&sb, "=== SEMANTIC SYMBOL TABLE ===\n");
    if (ctx->global_scope) {
        emit_scope(&sb, ctx->global_scope, 0);
    } else {
        sb_append_fmt(&sb, "No global scope initialized.\n");
    }
    sb_append_fmt(&sb, "=============================\n");
    
    return sb_free_and_return(&sb);
}

void semantic_to_file(SemanticCtx *ctx, const char *filename) {
    char *str = semantic_to_string(ctx);
    if (str) {
        FILE *f = fopen(filename, "w");
        if (f) {
            fputs(str, f);
            fclose(f);
        }
        free(str);
    }
}

