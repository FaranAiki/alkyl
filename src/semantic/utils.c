#include "semantic.h"

void mangle_type(char *buf, VarType t) {
    if (t.array_size > 0) {
        sprintf(buf + strlen(buf), "A%d_", t.array_size);
    }
    for(int i=0; i<t.ptr_depth; i++) strcat(buf, "P");
    
    switch(t.base) {
        case TYPE_INT: strcat(buf, "i"); break;
        case TYPE_DOUBLE: strcat(buf, "d"); break;
        case TYPE_FLOAT: strcat(buf, "f"); break;
        case TYPE_BOOL: strcat(buf, "b"); break;
        case TYPE_CHAR: strcat(buf, "c"); break;
        case TYPE_VOID: strcat(buf, "v"); break;
        case TYPE_STRING: strcat(buf, "s"); break;
        case TYPE_CLASS: 
            if (t.class_name)
                sprintf(buf + strlen(buf), "C%ld%s", strlen(t.class_name), t.class_name);
            else strcat(buf, "u");
            break;
        default: strcat(buf, "u"); break;
    }
}

void sem_error(SemCtx *ctx, ASTNode *node, const char *fmt, ...) {
    ctx->error_count++;

    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    Token t;
    t.line = node ? node->line : 0;
    t.col = node ? node->col : 0;
    t.text = NULL;
    
    Lexer l;
    if (ctx->source_code) {
        lexer_init(&l, ctx->source_code);
        l.filename = ctx->filename; 
    }
    
    report_error(ctx->source_code ? &l : NULL, t, msg);
}

char* mangle_function(const char *name, Parameter *params) {
    // Don't mangle main
    if (strcmp(name, "main") == 0) return strdup("main");

    char buf[1024];
    buf[0] = '\0';
    
    // Basic mangling scheme: _Z + len + name + params
    sprintf(buf, "_Z%ld%s", strlen(name), name);
    
    Parameter *p = params;
    while(p) {
        mangle_type(buf, p->type);
        p = p->next;
    }
    
    if (!params) strcat(buf, "v"); // void params
    
    return strdup(buf);
}

const char* type_to_str(VarType t) {
    static char buffers[4][128];
    static int idx = 0;
    char *buf = buffers[idx];
    idx = (idx + 1) % 4;

    const char *base;
    switch (t.base) {
        case TYPE_INT: base = "int"; break;
        case TYPE_CHAR: base = "char"; break;
        case TYPE_BOOL: base = "bool"; break;
        case TYPE_FLOAT: base = "single"; break;
        case TYPE_DOUBLE: base = "double"; break;
        case TYPE_VOID: base = "void"; break;
        case TYPE_STRING: base = "string"; break;
        case TYPE_CLASS: base = t.class_name ? t.class_name : "class"; break;
        case TYPE_UNKNOWN: base = "unknown"; break;
        case TYPE_AUTO: base = "auto"; break;
        default: base = "???"; break;
    }
    strcpy(buf, base);
    for(int i=0; i<t.ptr_depth; i++) strcat(buf, "*");
    if (t.array_size > 0) {
        char tmp[16]; sprintf(tmp, "[%d]", t.array_size);
        strcat(buf, tmp);
    }
    return buf;
}

const char* find_closest_type_name(SemCtx *ctx, const char *name) {
    const char *primitives[] = {
        "int", "char", "bool", "single", "double", "void", "string", "let", "auto", NULL
    };
    
    const char *best = NULL;
    int min_dist = 3; 

    for (int i = 0; primitives[i]; i++) {
        int d = levenshtein_dist(name, primitives[i]);
        if (d < min_dist) {
            min_dist = d;
            best = primitives[i];
        }
    }

    SemClass *c = ctx->classes;
    while(c) {
        int d = levenshtein_dist(name, c->name);
        if (d < min_dist) {
            min_dist = d;
            best = c->name;
        }
        c = c->next;
    }
    
    return best;
}

const char* find_closest_func_name(SemCtx *ctx, const char *name) {
    const char *builtins[] = {"print", "printf", "input", "malloc", "alloc", "free", "setjmp", "longjmp", NULL};
    const char *best = NULL;
    int min_dist = 3;

    for (int i = 0; builtins[i]; i++) {
        int d = levenshtein_dist(name, builtins[i]);
        if (d < min_dist) {
            min_dist = d;
            best = builtins[i];
        }
    }

    SemFunc *f = ctx->functions;
    while(f) {
        int d = levenshtein_dist(name, f->name);
        if (d < min_dist) {
            min_dist = d;
            best = f->name;
        }
        f = f->next;
    }
    return best;
}

const char* find_closest_var_name(SemCtx *ctx, const char *name) {
    const char *best = NULL;
    int min_dist = 3;
    
    Scope *scope = ctx->current_scope;
    while(scope) {
        SemSymbol *s = scope->symbols;
        while(s) {
            int d = levenshtein_dist(name, s->name);
            if (d < min_dist) {
                min_dist = d;
                best = s->name;
            }
            s = s->next;
        }
        scope = scope->parent;
    }
    return best;
}

void sem_info(SemCtx *ctx, ASTNode *node, const char *fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    Token t;
    t.line = node ? node->line : 0;
    t.col = node ? node->col : 0;
    t.text = NULL;
    
    Lexer l;
    if (ctx->source_code) {
        lexer_init(&l, ctx->source_code);
        l.filename = ctx->filename;
    }
    report_info(ctx->source_code ? &l : NULL, t, msg);
}

void sem_hint(SemCtx *ctx, ASTNode *node, const char *msg) {
    Token t;
    t.line = node ? node->line : 0;
    t.col = node ? node->col : 0;
    t.text = NULL;
    
    Lexer l;
    if (ctx->source_code) {
        lexer_init(&l, ctx->source_code);
        l.filename = ctx->filename;
    }
    report_hint(ctx->source_code ? &l : NULL, t, msg);
}

void sem_reason(SemCtx *ctx, int line, int col, const char *fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    Token t;
    t.line = line;
    t.col = col;
    t.text = NULL;
    
    Lexer l;
    if (ctx->source_code) {
        lexer_init(&l, ctx->source_code);
        l.filename = ctx->filename;
    }
    report_reason(ctx->source_code ? &l : NULL, t, msg);
}

void sem_suggestion(SemCtx *ctx, ASTNode *node, const char *suggestion) {
    Token t;
    t.line = node ? node->line : 0;
    t.col = node ? node->col : 0;
    t.text = NULL;
    
    Lexer l;
    if (ctx->source_code) {
        lexer_init(&l, ctx->source_code);
        l.filename = ctx->filename;
    }
    report_hint(ctx->source_code ? &l : NULL, t, suggestion);
}

int are_types_equal(VarType a, VarType b) {
    if (a.ptr_depth > 0 && b.ptr_depth > 0) {
        if (a.base == TYPE_VOID || b.base == TYPE_VOID) return 1;
    }

    if (a.base != b.base) {
        if (a.base == TYPE_AUTO || b.base == TYPE_AUTO) return 1;
        if (a.base == TYPE_STRING && b.base == TYPE_STRING) return 1;
        return 0;
    }
    if (a.ptr_depth != b.ptr_depth) return 0;
    if (a.base == TYPE_CLASS) {
        if (a.class_name && b.class_name) {
            return strcmp(a.class_name, b.class_name) == 0;
        }
        return 0;
    }
    return 1;
}

int get_conversion_cost(VarType from, VarType to) {
    if (are_types_equal(from, to)) return 0;
    
    if (from.ptr_depth == 0 && to.ptr_depth == 0) {
        // Widening (Safe) - Cost 1
        if (from.base == TYPE_INT && to.base == TYPE_DOUBLE) return 1;
        if (from.base == TYPE_INT && to.base == TYPE_FLOAT) return 1;
        if (from.base == TYPE_FLOAT && to.base == TYPE_DOUBLE) return 1;
        if (from.base == TYPE_CHAR && to.base == TYPE_INT) return 1;

        // Narrowing (Lossy) - Cost 2
        // Allows implicit cast but we can warn about it
        if (from.base == TYPE_DOUBLE && to.base == TYPE_INT) return 2;
        if (from.base == TYPE_FLOAT && to.base == TYPE_INT) return 2;
        if (from.base == TYPE_DOUBLE && to.base == TYPE_FLOAT) return 2;
        if (from.base == TYPE_INT && to.base == TYPE_CHAR) return 2;
    }
    
    if (from.base == TYPE_STRING && from.ptr_depth == 0) {
        if (to.base == TYPE_CHAR && to.ptr_depth == 1) return 1;
    }
    
    return -1;
}
