#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *fname = argv[1];
    char cmd[4096] = {0};
    char include_flags[1024] = {0};

    if (fname[0] == '/') {
        snprintf(cmd, sizeof(cmd), "echo '#include \"%s\"' | gcc -E -DWLR_USE_UNSTABLE -xc - 2>/dev/null", fname);
    } else {
        snprintf(cmd, sizeof(cmd), "echo '#include <%s>' | gcc -E -DWLR_USE_UNSTABLE -I. -xc - 2>/dev/null", fname);
    }

    if (fname[0] == '/') {
        char path_copy[512];
        strncpy(path_copy, fname, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';

        char *dir = path_copy;
        char *last_slash = strrchr(dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            for (int i = 0; i < 4 && dir[0] == '/' && strlen(dir) > 1; i++) {
                char flag[256];
                snprintf(flag, sizeof(flag), " -I%s", dir);
                if (strlen(include_flags) + strlen(flag) + 1 < sizeof(include_flags)) {
                    strcat(include_flags, flag);
                }
                last_slash = strrchr(dir, '/');
                if (last_slash && last_slash != dir) {
                    *last_slash = '\0';
                } else {
                    break;
                }
            }
        }
    }
    printf("%s%s\n", cmd, include_flags);
    return 0;
}
