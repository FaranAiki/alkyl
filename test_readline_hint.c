#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

void my_redisplay() {
    rl_redisplay(); // call standard redisplay
    
    // add hint
    if (rl_line_buffer && strlen(rl_line_buffer) > 0) {
        if (strncmp(rl_line_buffer, "f", 1) == 0) {
            char *hint = "aranaiki";
            printf("\033[90m%s\033[0m", hint);
            printf("\033[%dD", (int)strlen(hint));
            fflush(stdout);
        }
    }
}

int main() {
    rl_redisplay_function = my_redisplay;
    char *line = readline("> ");
    printf("Line: %s\n", line);
    return 0;
}
