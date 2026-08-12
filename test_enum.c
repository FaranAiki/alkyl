#include <stdio.h>
#include "include/parser/typestruct.h"
int main() {
    printf("TYPE_INT=%d TYPE_VOID=%d TYPE_NAMESPACE=%d TYPE_UNKNOWN=%d\n", TYPE_INT, TYPE_VOID, TYPE_NAMESPACE, TYPE_UNKNOWN);
    VarType t = {TYPE_INT, 0};
    printf("t.base=%d\n", t.base);
    return 0;
}
