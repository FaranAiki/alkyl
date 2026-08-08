#ifndef INTERN_H
#define INTERN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* intern_string(const char* str);
const char* intern_string_len(const char* str, size_t len);

#ifdef __cplusplus
}
#endif

#endif // INTERN_H
