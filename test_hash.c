#include <stdio.h>
#include <stdint.h>
#include <string.h>

uint32_t hash_string(const char *key) {
    uint32_t hash = 2166136261u;
    for (int i = 0; key[i] != '\0'; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

int main() {
    printf("Hash 'wlroots.Display': %u\n", hash_string("wlroots.Display"));
    printf("Hash 'Display': %u\n", hash_string("Display"));
    printf("Hash 'Backend': %u\n", hash_string("Backend"));
    printf("Hash 'wlroots.Backend': %u\n", hash_string("wlroots.Backend"));
    return 0;
}
