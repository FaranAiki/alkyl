#include <stdio.h>
unsigned int djb2(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}
int main() {
    printf("void: %u\n", djb2("void"));
    printf("long: %u\n", djb2("long"));
    return 0;
}
