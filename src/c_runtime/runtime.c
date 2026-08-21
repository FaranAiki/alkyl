#include <stdio.h>
#include <stdint.h>

/**
 * @brief Print a null-terminated string to stdout.
 * @param str String to print.
 */
void alkyl_print_str(const char* str) {
    printf("%s", str);
}

/**
 * @brief Print a single character to stdout.
 * @param c Character to print.
 */
void alkyl_print_char(char c) {
    printf("%c", c);
}

/**
 * @brief Print a signed 64-bit integer to stdout.
 * @param val Integer value to print.
 */
void alkyl_print_i64(int64_t val) {
    printf("%ld", val);
}

/**
 * @brief Print an unsigned 64-bit integer to stdout.
 * @param val Unsigned integer value to print.
 */
void alkyl_print_u64(uint64_t val) {
    printf("%lu", val);
}

/**
 * @brief Print a signed 32-bit integer to stdout.
 * @param val Integer value to print.
 */
void alkyl_print_i32(int32_t val) {
    printf("%d", val);
}

/**
 * @brief Print an unsigned 32-bit integer to stdout.
 * @param val Unsigned integer value to print.
 */
void alkyl_print_u32(uint32_t val) {
    printf("%u", val);
}

/**
 * @brief Print a 32-bit floating-point value to stdout.
 * @param val Float value to print.
 */
void alkyl_print_f32(float val) {
    printf("%f", val);
}

/**
 * @brief Print a 64-bit floating-point value to stdout.
 * @param val Double value to print.
 */
void alkyl_print_f64(double val) {
    printf("%lf", val);
}
