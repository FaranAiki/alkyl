#include "keyboard.h"
#include <stdbool.h>
#include "common/context.h"
#include "semantic/typestruct.h"
#include "../common/arena.h"
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "common/common.h"

struct SemanticCtx;
struct SemSymbol;
SemSymbol* sem_symbol_lookup(SemanticCtx *ctx, const char *name, SemScope **out_scope);

#define MAX_HISTORY 100
#define MAX_INPUT_LEN 4096

static char cmd_history[MAX_HISTORY][MAX_INPUT_LEN];
static int cmd_history_count = 0;
static int history_view_idx = 0;

static void add_to_cmd_history(const char *line) {
    if (strlen(line) == 0) return;
    if (cmd_history_count > 0 && streq(cmd_history[cmd_history_count - 1], line)) return;
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

static int has_unbalanced_braces(const char *buffer, int len) {
    int brace = 0;
    int in_str = 0, in_char = 0;
    for (int i = 0; i < len; i++) {
        if (buffer[i] == '"' && !in_char) in_str = !in_str;
        else if (buffer[i] == '\'' && !in_str) in_char = !in_char;
        else if (!in_str && !in_char) {
            if (buffer[i] == '{') brace++;
            else if (buffer[i] == '}') brace--;
        }
    }
    return brace > 0;
}

static void cursor_up(char *buffer, int len, int *pos) {
    int current_line_start = 0;
    for (int i = *pos - 1; i >= 0; i--) {
        if (buffer[i] == '\n') {
            current_line_start = i + 1;
            break;
        }
    }
    if (current_line_start == 0) return;

    int prev_line_start = 0;
    for (int i = current_line_start - 2; i >= 0; i--) {
        if (buffer[i] == '\n') {
            prev_line_start = i + 1;
            break;
        }
    }

    int col_offset = *pos - current_line_start;
    int prev_line_end = current_line_start - 1;
    int prev_line_len = prev_line_end - prev_line_start;
    if (prev_line_len < 0) prev_line_len = 0;

    if (col_offset <= prev_line_len) {
        *pos = prev_line_start + col_offset;
    } else {
        *pos = prev_line_start + prev_line_len;
    }
}

static void cursor_down(char *buffer, int len, int *pos) {
    int current_line_start = 0;
    for (int i = *pos - 1; i >= 0; i--) {
        if (buffer[i] == '\n') {
            current_line_start = i + 1;
            break;
        }
    }

    int next_newline = len;
    for (int i = *pos; i < len; i++) {
        if (buffer[i] == '\n') {
            next_newline = i;
            break;
        }
    }

    if (next_newline >= len) return;

    int next_line_start = next_newline + 1;
    if (next_line_start >= len) return;

    int col_offset = *pos - current_line_start;
    int next_line_end = len;
    for (int i = next_line_start; i < len; i++) {
        if (buffer[i] == '\n') {
            next_line_end = i;
            break;
        }
    }
    int next_line_len = next_line_end - next_line_start;

    if (col_offset <= next_line_len) {
        *pos = next_line_start + col_offset;
    } else {
        *pos = next_line_start + next_line_len;
    }
}

static void insert_newline_line(char *buffer, int *len, int *pos) {
    int current_brace = 0;
    for (int i = 0; i < *pos; i++) {
        if (buffer[i] == '{') current_brace++;
        if (buffer[i] == '}') current_brace--;
    }
    if (current_brace < 0) current_brace = 0;

    int insert_len = 1 + current_brace * 4;
    if (*len + insert_len < MAX_INPUT_LEN - 1) {
        for (int j = *len; j >= *pos; j--) buffer[j + insert_len] = buffer[j];
        buffer[*pos] = '\n';
        for (int j = 1; j <= current_brace * 4; j++) buffer[*pos + j] = ' ';
        *len += insert_len;
        *pos += insert_len;
    }
}

static void cursor_word_left(char *buffer, int len, int *pos) {
    if (*pos == 0) return;
    int i = *pos - 1;
    while (i >= 0 && !isalnum((unsigned char)buffer[i]) && buffer[i] != '_') i--;
    while (i >= 0 && (isalnum((unsigned char)buffer[i]) || buffer[i] == '_')) i--;
    *pos = i + 1;
}

static void cursor_word_right(char *buffer, int len, int *pos) {
    if (*pos >= len) return;
    int i = *pos;
    while (i < len && !isalnum((unsigned char)buffer[i]) && buffer[i] != '_') i++;
    while (i < len && (isalnum((unsigned char)buffer[i]) || buffer[i] == '_')) i++;
    *pos = i;
}

static void redraw(const char *base_prompt, const char *base_prompt_no_color, const char *buffer, int len, int pos, const char *suggestion, int word_len, int *last_cursor_row) {
    if (*last_cursor_row > 0) {
        printf("\033[%dA", *last_cursor_row);
    }
    printf("\r\033[K\033[J"); // clear line and from cursor to end of screen

    int base_prompt_len = strlen(base_prompt_no_color);
    
    int target_row = 0;
    int target_col = 0;
    
    int line_start = 0;
    int current_row = 0;
    
    for (int i = 0; i <= len; i++) {
        if (i == pos) {
            target_row = current_row;
            target_col = i - line_start;
        }
        
        if (i == len || buffer[i] == '\n') {
            if (current_row == 0) {
                printf("%s", base_prompt);
            } else {
                printf("... ");
                int padding = base_prompt_len - 4;
                for (int p = 0; p < padding; p++) printf(" ");
            }
            
            for (int j = line_start; j < i; j++) {
                putchar(buffer[j]);
            }
            
            if (suggestion != NULL && pos == len && i == len && word_len > 0) {
                printf("\033[90m%s\033[0m", suggestion + word_len);
            }
            
            if (i < len) {
                printf("\n");
            }
            
            line_start = i + 1;
            current_row++;
        }
    }
    
    int total_rows = current_row;
    
    int rows_up = (total_rows - 1) - target_row;
    if (rows_up > 0) {
        printf("\033[%dA", rows_up);
    }
    
    printf("\r");
    if (base_prompt_len + target_col > 0) {
        printf("\033[%dC", base_prompt_len + target_col);
    }
    
    *last_cursor_row = target_row;
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

char* get_smart_input(void *arena, int cmd_count, void *sem_ctx) {
    if (!isatty(STDIN_FILENO)) {
        return get_smart_input_piped(arena, cmd_count);
    }

    history_view_idx = cmd_history_count;

    char *input_buffer = arena_alloc(arena, MAX_INPUT_LEN);
    if (!input_buffer) return NULL;
    input_buffer[0] = '\0';

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_iflag &= ~(ICRNL | INLCR | IGNCR);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    int len = 0;
    int pos = 0;
    char temp_buffer[MAX_INPUT_LEN] = "";
    int last_cursor_row = 0;

    char base_prompt[128];
    snprintf(base_prompt, sizeof(base_prompt), "\033[32mIn [%d]:\033[0m ", cmd_count);
    char base_prompt_no_color[128];
    snprintf(base_prompt_no_color, sizeof(base_prompt_no_color), "In [%d]: ", cmd_count);

    while (1) {
        int current_line_start = 0;
        for (int i = pos - 1; i >= 0; i--) {
            if (input_buffer[i] == '\n') {
                current_line_start = i + 1;
                break;
            }
        }

        int word_len = 0;
        char *suggestion = NULL;
        int word_start = current_line_start;
        
        if (pos == len) {
            for (int i = pos - 1; i >= current_line_start; i--) {
                if (!isalnum(input_buffer[i]) && input_buffer[i] != '_') {
                    word_start = i + 1;
                    break;
                }
            }
            word_len = pos - word_start;
            if (word_len > 0) {
                if (sem_ctx != NULL) {
                    SemanticCtx *sem = (SemanticCtx*)sem_ctx;
                    SemScope *scope = sem->current_scope;
                    
                    if (word_start > 0 && input_buffer[word_start - 1] == '.') {
                        int prefix_start = 0;
                        for (int i = word_start - 2; i >= current_line_start; i--) {
                            if (!isalnum((unsigned char)input_buffer[i]) && input_buffer[i] != '_' && input_buffer[i] != '.') {
                                prefix_start = i + 1;
                                break;
                            }
                        }
                        if (prefix_start < word_start - 1) {
                            char prefix[128] = {0};
                            int p_len = word_start - 1 - prefix_start;
                            if (p_len < 127) strncpy(prefix, input_buffer + prefix_start, p_len);
                            SemSymbol *ns_sym = sem_symbol_lookup(sem, prefix, NULL);
                            if (ns_sym && ns_sym->inner_scope) {
                                scope = ns_sym->inner_scope;
                            } else {
                                scope = NULL;
                            }
                        } else {
                            scope = NULL;
                        }
                    }

                    while (scope) {
                        SemSymbol *sym = scope->symbols;
                        while (sym) {
                            if (!sym->name) { sym = sym->next; continue; }
                            int sym_len = strlen(sym->name);
                            if (sym_len > word_len && strncmp(input_buffer + word_start, sym->name, word_len) == 0) {
                                suggestion = sym->name;
                                break;
                            }
                            sym = sym->next;
                        }
                        if (suggestion != NULL) break;
                        if (word_start > 0 && input_buffer[word_start - 1] == '.') break; // only search the exact namespace/class
                        scope = scope->parent;
                    }
                }

                if (suggestion == NULL) {
                    const char *keywords[] = {
                        "let", "mut", "if", "else", "while", "for", "in", "return", "switch", "case",
                        "break", "continue", "func", "class", "struct", "union", "enum", "errnum",
                        "import", "namespace", "true", "false", "null", "void", "extern", "pure", "pristine", NULL
                    };
                    for (int j = 0; keywords[j]; j++) {
                        int kw_len = strlen(keywords[j]);
                        if (kw_len > word_len && strncmp(input_buffer + word_start, keywords[j], word_len) == 0) {
                            suggestion = (char*)keywords[j];
                            break;
                        }
                    }
                }
            }
        }

        redraw(base_prompt, base_prompt_no_color, input_buffer, len, pos, suggestion, word_len, &last_cursor_row);

        char c = getchar();

        if (c == '\n') {
            insert_newline_line(input_buffer, &len, &pos);
            if (history_view_idx == cmd_history_count) {
                strcpy(temp_buffer, input_buffer);
            }
        } else if (c == '\r') {
            int paren = 0, bracket = 0, brace = 0;
            int in_str = 0, in_char = 0;
            for (int i = 0; i < len; i++) {
                if (input_buffer[i] == '"' && !in_char) in_str = !in_str;
                else if (input_buffer[i] == '\'' && !in_str) in_char = !in_char;
                else if (!in_str && !in_char) {
                    if (input_buffer[i] == '(') paren++;
                    else if (input_buffer[i] == ')') paren--;
                    else if (input_buffer[i] == '[') bracket++;
                    else if (input_buffer[i] == ']') bracket--;
                    else if (input_buffer[i] == '{') brace++;
                    else if (input_buffer[i] == '}') brace--;
                }
            }

            if (paren <= 0 && bracket <= 0 && brace <= 0 && !in_str && !in_char) {
                redraw(base_prompt, base_prompt_no_color, input_buffer, len, pos, NULL, 0, &last_cursor_row);

                int total_rows = 1;
                for(int i = 0; i < len; i++) if (input_buffer[i] == '\n') total_rows++;
                int rows_down = (total_rows - 1) - last_cursor_row;
                if (rows_down > 0) {
                    printf("\033[%dB", rows_down);
                }
                printf("\n");

                input_buffer[len] = '\0';
                while (len > 0 && input_buffer[len - 1] == ' ') {
                    input_buffer[--len] = '\0';
                }

                if (len > 0) {
                    add_to_cmd_history(input_buffer);
                }
                tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                return input_buffer;
            } else {
                insert_newline_line(input_buffer, &len, &pos);
                if (history_view_idx == cmd_history_count) {
                    strcpy(temp_buffer, input_buffer);
                }
            }
        } else if (c == 127 || c == 8) {
            if (pos > 0) {
                for (int j = pos - 1; j < len; j++) input_buffer[j] = input_buffer[j + 1];
                len--;
                pos--;
                if (history_view_idx == cmd_history_count) {
                    strcpy(temp_buffer, input_buffer);
                }
            }
        } else if (c == 9) { // Tab
            if (suggestion != NULL) {
                strcpy(input_buffer + word_start, suggestion);
                int added_len = strlen(suggestion) - word_len;
                if (len + added_len + 1 < MAX_INPUT_LEN - 1) {
                    for(int j = len; j >= pos; j--) input_buffer[j + added_len + 1] = input_buffer[j];
                    input_buffer[pos + added_len] = ' ';
                    len += added_len + 1;
                    pos += added_len + 1;
                }
            } else {
                if (len + 4 < MAX_INPUT_LEN - 1) {
                    for (int i = 0; i < 4; i++) {
                        for (int j = len; j > pos; j--) input_buffer[j] = input_buffer[j - 1];
                        input_buffer[pos] = ' ';
                        len++;
                        pos++;
                    }
                    if (history_view_idx == cmd_history_count) {
                        strcpy(temp_buffer, input_buffer);
                    }
                }
            }
        } else if (c == 27) {
            char seq1 = getchar();
            if (seq1 == '\r' || seq1 == '\n') { // Alt+Enter
                insert_newline_line(input_buffer, &len, &pos);
                if (history_view_idx == cmd_history_count) {
                    strcpy(temp_buffer, input_buffer);
                }
            } else if (seq1 == '[' || seq1 == 'O') {
                char seq2 = getchar();
                if (seq1 == 'O' && seq2 == 'M') { // Shift+Enter / Keypad Enter
                    insert_newline_line(input_buffer, &len, &pos);
                    if (history_view_idx == cmd_history_count) {
                        strcpy(temp_buffer, input_buffer);
                    }
                } else if (seq2 == 'D') { // Left
                    if (pos > 0) pos--;
                } else if (seq2 == 'C') { // Right
                    if (pos < len) pos++;
                    else if (pos == len && suggestion != NULL) {
                        strcpy(input_buffer + word_start, suggestion);
                        int added_len = strlen(suggestion) - word_len;
                        if (len + added_len < MAX_INPUT_LEN - 1) {
                            len += added_len;
                            pos = len;
                        }
                    }
                } else if (seq2 == '1') {
                    char seq3 = getchar();
                    if (seq3 == ';') {
                        char seq4 = getchar();
                        if (seq4 == '5') {
                            char seq5 = getchar();
                            if (seq5 == 'D') { // Ctrl+Left
                                cursor_word_left(input_buffer, len, &pos);
                            } else if (seq5 == 'C') { // Ctrl+Right
                                cursor_word_right(input_buffer, len, &pos);
                            }
                        } else if (seq4 == '2') {
                            char seq5 = getchar();
                            if (seq5 == 'A') { // Shift+Up
                                cursor_up(input_buffer, len, &pos);
                            } else if (seq5 == 'B') { // Shift+Down
                                cursor_down(input_buffer, len, &pos);
                            }
                        }
                    } else if (seq3 == '3') {
                        char seq4 = getchar();
                        if (seq4 == ';') {
                            char seq5 = getchar();
                            if (seq5 == '2') {
                                char seq6 = getchar();
                                if (seq6 == 'u') { // Shift+Enter (\e[13;2u)
                                    insert_newline_line(input_buffer, &len, &pos);
                                    if (history_view_idx == cmd_history_count) {
                                        strcpy(temp_buffer, input_buffer);
                                    }
                                }
                            }
                        }
                    }
                } else if (seq2 == 'A') { // Up
                    if (has_unbalanced_braces(input_buffer, len)) {
                        cursor_up(input_buffer, len, &pos);
                    } else {
                        if (history_view_idx > 0) {
                            if (history_view_idx == cmd_history_count) {
                                strcpy(temp_buffer, input_buffer);
                            }
                            history_view_idx--;
                            strcpy(input_buffer, cmd_history[history_view_idx]);
                            len = strlen(input_buffer);
                            pos = len;
                        }
                    }
                } else if (seq2 == 'B') { // Down
                    if (has_unbalanced_braces(input_buffer, len)) {
                        cursor_down(input_buffer, len, &pos);
                    } else {
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
                    }
                } else if (seq2 == 'H') {
                    pos = current_line_start;
                } else if (seq2 == 'F') {
                    int next_newline = len;
                    for (int i = pos; i < len; i++) {
                        if (input_buffer[i] == '\n') {
                            next_newline = i;
                            break;
                        }
                    }
                    pos = next_newline;
                } else if (seq2 == '3') {
                    char seq3 = getchar();
                    if (seq3 == '~' && pos < len) {
                        for (int j = pos; j < len; j++) input_buffer[j] = input_buffer[j + 1];
                        len--;
                    }
                }
            }
        } else if (c >= 32 && c <= 126) {
            if (c == '}' && pos > current_line_start) {
                int only_spaces = 1;
                for (int i = current_line_start; i < pos; i++) {
                    if (input_buffer[i] != ' ') {
                        only_spaces = 0;
                        break;
                    }
                }
                if (only_spaces) {
                    int spaces = pos - current_line_start;
                    int to_delete = spaces >= 4 ? 4 : spaces;
                    if (to_delete > 0) {
                        for (int j = pos; j < len; j++) input_buffer[j - to_delete] = input_buffer[j];
                        len -= to_delete;
                        pos -= to_delete;
                    }
                }
            }

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
    }
}