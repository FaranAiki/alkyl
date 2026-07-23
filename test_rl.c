#include <readline/readline.h>
#include <stdio.h>
static int accept_line_clear_hint(int count, int key) {
    printf("\033[K");
    return rl_newline(count, key);
}
int main() {
    rl_bind_key('\r', accept_line_clear_hint);
    return 0;
}
