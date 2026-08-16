#ifndef INTERN_H
#define INTERN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Interns a null-terminated string globally.
 * @param str The string to intern.
 * @return A canonical pointer to the interned string, or NULL on failure.
 */
const char* intern_string(const char* str);
/**
 * @brief Interns a string of given length globally.
 * @param str The string buffer to intern.
 * @param len The length of the string.
 * @return A canonical pointer to the interned string, or NULL on failure.
 */
const char* intern_string_len(const char* str, size_t len);

#ifdef __cplusplus
}
#endif

#endif // INTERN_H
