#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StringBuilder;

void sb_init(StringBuilder *sb);
void sb_append_fmt(StringBuilder *sb, const char *fmt, ...);
char* sb_free_and_return(StringBuilder *sb);
void sb_append(StringBuilder *sb, const char *str); 
void sb_append_escaped(StringBuilder *sb, const char *str);

char* escape_string(const char* input);

#endif // COMMON_H
