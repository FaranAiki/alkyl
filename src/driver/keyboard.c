#include "keyboard.h"
#include "../common/arena.h"
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_HISTORY 100
#define MAX_INPUT_LEN 4096

static char cmd_history[MAX_HISTORY][MAX_INPUT_LEN];
static int cmd_history_count = 0;
static int history_view_idx = 0;

static void add_to_cmd_history(const char *line) {
    if (strlen(line) == 0) return;
    if (cmd_history_count > 0 && strcmp(cmd_history[cmd_history_count - 1], line) == 0) return;
    if (cmd_history_count < MAX_HISTORY) {
        strcpy(cmd_history[cmd_history_count], line);
        cmd_history_count++;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(cmd_history[i], cmd_history[i + 1]);
        }
        strcpy(cmd_history[MAX_HISTORY - 1], line);
    }
}

static void redraw(const char *prompt, const char *buffer, int len, int pos, const char *suggestion) {
    printf("\r\033[K%s%s", prompt, buffer);
    int end_pos = (suggestion != NULL) ? (int)strlen(suggestion) : len;
    int move_back = end_pos - pos;
    if (move_back > 0) {
        printf("\033[%dD", move_back);
    }
    if (suggestion != NULL && pos == len && len > 0) {
        printf("\033[90m%s\033[0m", suggestion + len);
    }
    fflush(stdout);
}

static char* get_smart_input_piped(void *arena, int cmd_count) {
    char prompt[128];
    snprintf(prompt, sizeof(prompt), "\033[32mIn [%d]:\033[0m ", cmd_count);

    char *input_buffer = arena_alloc(arena, MAX_INPUT_LEN);
    if (!input_buffer) return NULL;
    input_buffer[0] = '\0';

    printf("%s", prompt);
    fflush(stdout);

    if (fgets(input_buffer, MAX_INPUT_LEN, stdin) == NULL) {
        return NULL;
    }

    int len = strlen(input_buffer);
    while (len > 0 && (input_buffer[len - 1] == '\n' || input_buffer[len - 1] == '\r')) {
        input_buffer[--len] = '\0';
    }

    return input_buffer;
}

char* get_smart_input(void *arena, int cmd_count) {
    if (!isatty(STDIN_FILENO)) {
        return get_smart_input_piped(arena, cmd_count);
    }

    char prompt[128];
    snprintf(prompt, sizeof(prompt), "\033[32mIn [%d]:\033[0m ", cmd_count);

    char *input_buffer = arena_alloc(arena, MAX_INPUT_LEN);
    if (!input_buffer) return NULL;
    input_buffer[0] = '\0';

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    int brace_depth = 0;
    int in_indent_block = 0;
    int first_line = 1;
    int len = 0;
    int pos = 0;
    char temp_buffer[MAX_INPUT_LEN] = "";

    while (1) {
        char cur_prompt[192];
        if (!first_line && in_indent_block) {
            snprintf(cur_prompt, sizeof(cur_prompt), "... ");
            for (int i = 0; i < brace_depth && i < 10; i++) {
                strcat(cur_prompt, "    ");
            }
        } else {
            snprintf(cur_prompt, sizeof(cur_prompt), "\033[32mIn [%d]:\033[0m ", cmd_count);
        }

        char *suggestion = NULL;
        if (len > 0 && pos == len && first_line) {
            const char *keywords[] = {
                "let", "mut", "if", "else", "while", "for", "in", "return", "switch", "case",
                "break", "continue", "func", "class", "struct", "union", "enum", "errnum",
                "import", "namespace", "true", "false", "null", "void", "extern", "pure", "pristine", NULL
            };
            for (int j = 0; keywords[j]; j++) {
                int kw_len = strlen(keywords[j]);
                if (kw_len > len && strncmp(input_buffer, keywords[j], len) == 0) {
                    suggestion = keywords[j];
                    break;
                }
            }
        }

        redraw(cur_prompt, input_buffer, len, pos, suggestion);

        char c = getchar();

        if (c == '\n' || c == '\r') {
            input_buffer[len] = '\0';
            while (len > 0 && input_buffer[len - 1] == ' ') {
                input_buffer[--len] = '\0';
            }
            printf("\n");
            if (first_line && len > 0) {
                add_to_cmd_history(input_buffer);
            }
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return input_buffer;
        } else if (c == 127 || c == 8) {
            if (pos > 0) {
                for (int j = pos - 1; j < len; j++) input_buffer[j] = input_buffer[j + 1];
                len--;
                pos--;
                if (history_view_idx == cmd_history_count) {
                    strcpy(temp_buffer, input_buffer);
                }
            }
        } else if (c == 9) {
            if (suggestion != NULL) {
                strcpy(input_buffer, suggestion);
                len = strlen(input_buffer);
                if (len < MAX_INPUT_LEN - 1) {
                    input_buffer[len++] = ' ';
                    input_buffer[len] = '\0';
                }
                pos = len;
            }
        } else if (c == 27) {
            char seq1 = getchar();
            if (seq1 == '[') {
                char seq2 = getchar();
                if (seq2 == 'D') {
                    if (pos > 0) pos--;
                } else if (seq2 == 'C') {
                    if (pos < len) pos++;
                } else if (seq2 == 'A') {
                    if (history_view_idx > 0) {
                        if (history_view_idx == cmd_history_count) {
                            strcpy(temp_buffer, input_buffer);
                        }
                        history_view_idx--;
                        strcpy(input_buffer, cmd_history[history_view_idx]);
                        len = strlen(input_buffer);
                        pos = len;
                    }
                } else if (seq2 == 'B') {
                    if (history_view_idx < cmd_history_count) {
                        history_view_idx++;
                        if (history_view_idx == cmd_history_count) {
                            strcpy(input_buffer, temp_buffer);
                        } else {
                            strcpy(input_buffer, cmd_history[history_view_idx]);
                        }
                        len = strlen(input_buffer);
                        pos = len;
                    }
                } else if (seq2 == 'H') {
                    pos = 0;
                } else if (seq2 == 'F') {
                    pos = len;
                } else if (seq2 == '3') {
                    char seq3 = getchar();
                    if (seq3 == '~' && pos < len) {
                        for (int j = pos; j < len; j++) input_buffer[j] = input_buffer[j + 1];
                        len--;
                    }
                }
            }
        } else if (c >= 32 && c <= 126) {
            if (len < MAX_INPUT_LEN - 1) {
                for (int j = len; j > pos; j--) input_buffer[j] = input_buffer[j - 1];
                input_buffer[pos] = c;
                len++;
                pos++;
                input_buffer[len] = '\0';
                if (history_view_idx == cmd_history_count) {
                    strcpy(temp_buffer, input_buffer);
                }
            }
        }

        if (first_line && !in_indent_block) {
            int ld = 0;
            int ins = 0;
            int inc = 0;
            for (int i = 0; i < len; i++) {
                if (input_buffer[i] == '"' && !inc) ins = !ins;
                if (input_buffer[i] == '\'' && !ins) inc = !inc;
                if (!ins && !inc) {
                    if (input_buffer[i] == '{') ld++;
                    if (input_buffer[i] == '}') ld--;
                }
            }
            brace_depth = ld;

            if (brace_depth == 0 && !ins && !inc) {
                const char *trimmed = input_buffer;
                while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
                if (strncmp(trimmed, "if ", 3) == 0 || strncmp(trimmed, "if(", 3) == 0 ||
                    strncmp(trimmed, "while ", 6) == 0 || strncmp(trimmed, "while(", 6) == 0 ||
                    strncmp(trimmed, "for ", 4) == 0 || strncmp(trimmed, "for(", 4) == 0 ||
                    strncmp(trimmed, "else", 4) == 0 ||
                    strncmp(trimmed, "func ", 5) == 0 || strncmp(trimmed, "class ", 6) == 0 ||
                    strncmp(trimmed, "struct ", 7) == 0 ||
                    strncmp(trimmed, "flux ", 5) == 0) {
                    if (len > 0 && trimmed[strlen(trimmed) - 1] != ';' && trimmed[strlen(trimmed) - 1] != '}') {
                        in_indent_block = 1;
                    }
                }
            }
        }

        if (first_line && brace_depth == 0 && !in_indent_block) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return input_buffer;
        }

        if (!first_line && in_indent_block && len == 0) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return input_buffer;
        }

        first_line = 0;
    }
}