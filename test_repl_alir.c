#include <stdlib.h>
#include <stdio.h>
int main() {
    system("build/alkyl test_repl2.in");
    system("cat out.alir | grep -A 20 \"__expr_0\"");
    return 0;
}
