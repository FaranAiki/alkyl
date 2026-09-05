#include <stdio.h>
#include <string.h>

int main() {
    char include_flags[1024] = {0};
    char *fname = "wlr/types/wlr_xdg_shell.h";
    // run the pkg-config block manually
    FILE *pf = popen("pkg-config --cflags wlroots-0.18 2>/dev/null", "r");
    if (pf) {
        char pkg_out[512] = {0};
        size_t pkg_len = fread(pkg_out, 1, sizeof(pkg_out) - 1, pf);
        pkg_out[pkg_len] = '\0';
        pclose(pf);
        if (pkg_len > 0) {
            char *p = pkg_out;
            while (*p) {
                while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                if (*p == '-' && *(p+1) == 'I') {
                    p += 2;
                    char *end = p;
                    while (*end && *end != ' ' && *end != '\t' && *end != '\n') end++;
                    size_t path_len = end - p;
                    if (path_len > 0 && path_len < 256) {
                        char flag[256];
                        snprintf(flag, sizeof(flag), " -I%.*s", (int)path_len, p);
                        strcat(include_flags, flag);
                    }
                    p = end;
                } else {
                    while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
                }
            }
        }
    }
    if (include_flags[0]) { strcat(include_flags, " -I/usr/local/share/alkyl/wlroots"); }
    printf("include_flags: %s\n", include_flags);
    return 0;
}
