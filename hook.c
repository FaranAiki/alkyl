#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

void sem_error(void *ctx, void *node, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "INTERCEPTED ERROR: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}
