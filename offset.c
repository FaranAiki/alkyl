#include <stdio.h>
#include <stddef.h>
#include "alir/alir.h"

int main() {
    printf("sizeof(AlirValue) = %zu\n", sizeof(AlirValue));
    printf("offsetof(AlirValue, val) = %zu\n", offsetof(AlirValue, val));
    return 0;
}
