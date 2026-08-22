#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *f = popen("./build/ethyl --debug project/wmyl/wmyl.kyl --unopt", "r");
    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        printf("%s", buf);
    }
    pclose(f);
    return 0;
}
