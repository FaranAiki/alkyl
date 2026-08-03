#include <stdio.h>
#include <stdint.h>

void alkyl_print_str(const char* str) {
    printf("%s", str);
}

void alkyl_print_char(char c) {
    printf("%c", c);
}

void alkyl_print_i64(int64_t val) {
    printf("%ld", val);
}

void alkyl_print_u64(uint64_t val) {
    printf("%lu", val);
}

void alkyl_print_i32(int32_t val) {
    printf("%d", val);
}

void alkyl_print_u32(uint32_t val) {
    printf("%u", val);
}

void alkyl_print_f32(float val) {
    printf("%f", val);
}

void alkyl_print_f64(double val) {
    printf("%lf", val);
}
