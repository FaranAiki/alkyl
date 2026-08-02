#include <stdio.h>
#include <stddef.h>
#include "include/alir/alir.h"
int main() {
    printf("offsetof param_count: %zu\n", offsetof(AlirFunction, param_count));
    printf("offsetof params: %zu\n", offsetof(AlirFunction, params));
    printf("offsetof blocks: %zu\n", offsetof(AlirFunction, blocks));
    return 0;
}
