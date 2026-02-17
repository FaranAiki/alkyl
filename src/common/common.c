#include "common.h"

void sb_init(StringBuilder *sb) {
    sb->cap = 2048; // Increased initial cap
    sb->len = 0;
    sb->data = malloc(sb->cap);
    if (sb->data) sb->data[0] = '\0';
}

void sb_append(StringBuilder *sb, const char *str) {
    if (!str || !sb->data) return;
    size_t slen = strlen(str);
    if (sb->len + slen + 1 >= sb->cap) {
        sb->cap = (sb->cap * 2) + slen;
        char *new_data = realloc(sb->data, sb->cap);
        if (!new_data) { free(sb->data); sb->data = NULL; return; }
        sb->data = new_data;
    }
    strcpy(sb->data + sb->len, str);
    sb->len += slen;
}

void sb_append_fmt(StringBuilder *sb, const char *fmt, ...) {
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

// Helper to escape characters for string literals
void sb_append_escaped(StringBuilder *sb, const char *str) {
    if (!str) return;
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '\n': sb_append(sb, "\\n"); break;
            case '\t': sb_append(sb, "\\t"); break;
            case '\r': sb_append(sb, "\\r"); break;
            case '\\': sb_append(sb, "\\\\"); break;
            case '\"': sb_append(sb, "\\\""); break;
            default: {
                char tmp[2] = {*p, 0};
                sb_append(sb, tmp);
            }
        }
    }
}

char* sb_free_and_return(StringBuilder *sb) {
    return sb->data; 
}

char* escape_string(const char* input) {
    if (!input) return strdup("");
    int len = strlen(input);
    char* out = malloc(len * 2 + 1); // Worst case
    char* p = out;
    for (int i = 0; i < len; i++) {
        if (input[i] == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (input[i] == '\t') { *p++ = '\\'; *p++ = 't'; }
        else if (input[i] == '\"') { *p++ = '\\'; *p++ = '\"'; }
        else if (input[i] == '\\') { *p++ = '\\'; *p++ = '\\'; }
        else *p++ = input[i];
    }
    *p = '\0';
    return out;
}
