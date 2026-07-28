#include "../../header/helper/color.h"
#include "keyboard.h"
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Terminal Colors for Prompt */

/* TODO: apakah harus menggunakan global variabel? */
boolean EndKata;
Kata CKata;

char CC;
boolean EOP;

/* Commands for autofill - now lowercase */
char *commands[] = {
    "discover", "search", "open", "open_title", "openlinked", "execute", "import", "cache", "new_tab", "close_tab", "next_tab", "prev_tab", "list_tabs", "back", "forward", "back_n", "forward_n", "history", "graph", "download", "help", "how", "clear", "add_page", "edit_page", "delete_page", "add_link", "exit"
};
int num_commands = 28;

/* Command History Storage */
#define MAX_HISTORY_STORAGE 100
static char cmd_history[MAX_HISTORY_STORAGE][256];
static int cmd_history_count = 0;

static void add_to_cmd_history(const char* line) {
    if (strlen(line) == 0) return;
    // Don't add if identical to last command
    if (cmd_history_count > 0 && strcmp(cmd_history[cmd_history_count - 1], line) == 0) return;

    if (cmd_history_count < MAX_HISTORY_STORAGE) {
        strcpy(cmd_history[cmd_history_count], line);
        cmd_history_count++;
    } else {
        // Shift history
        for (int i = 0; i < MAX_HISTORY_STORAGE - 1; i++) {
            strcpy(cmd_history[i], cmd_history[i + 1]);
        }
        strcpy(cmd_history[MAX_HISTORY_STORAGE - 1], line);
    }
}

/* Helper for case-insensitive comparison */
static int case_insensitive_compare(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        if (tolower((unsigned char)s1[i]) != tolower((unsigned char)s2[i])) {
            return (unsigned char)tolower((unsigned char)s1[i]) - (unsigned char)tolower((unsigned char)s2[i]);
        }
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

/* TODO: apakah harus menggunakan global variabel? */
static FILE * pita;
static int retval;

void getLineWithAutofill(char *buffer, int max_len, const char *current_tab, const char *current_website, int execution_count) {
    struct termios oldt, newt;
    int len = 0;
    int pos = 0;
    char c;
    char *suggestion = NULL;

    // History navigation state
    int history_view_idx = cmd_history_count; // Start at the "new" entry
    char temp_buffer[256] = ""; // To store what user typed before moving Up

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    buffer[0] = '\0';

    while (1) {
        // Find suggestion (only if cursor is at the end)
        suggestion = NULL;
        if (len > 0 && pos == len) {
            for (int j = 0; j < num_commands; j++) {
                if (case_insensitive_compare(buffer, commands[j], len) == 0) {
                    suggestion = commands[j];
                    break;
                }
            }
        }

        // Redraw line
        if (current_website != NULL) {
            printf("\r\033[K" BOLD BLUE "[ " RESET GREEN "%s" RESET " | " YELLOW "%s" RESET " | " GREEN "%d" BOLD BLUE " ]: " RESET "%s",
                   current_tab, current_website, execution_count, buffer);
        } else {
            printf("\r\033[K" BOLD BLUE "[ " RESET GREEN "%s" RESET " | " GREEN "%d" BOLD BLUE " ]: " RESET "%s",
                   current_tab, execution_count, buffer);
        }

        // Ghost text
        if (suggestion != NULL) {
            printf("\033[90m%s\033[0m", suggestion + len);
        }

        // Move cursor back to pos
        int current_end_pos = (suggestion != NULL ? (int)strlen(suggestion) : len);
        int move_back = current_end_pos - pos;
        if (move_back > 0) {
            printf("\033[%dD", move_back);
        }

        fflush(stdout);

        c = getchar();

        if (c == '\n' || c == '\r') {
            buffer[len] = '\0';
            // Trim trailing spaces
            while (len > 0 && buffer[len-1] == ' ') {
                buffer[--len] = '\0';
            }
            if (current_website != NULL) {
                printf("\r\033[K" BOLD BLUE "[ " RESET GREEN "%s" RESET " | " YELLOW "%s" RESET " | " GREEN "%d" BOLD BLUE " ]: " RESET "%s\n",
                       current_tab, current_website, execution_count, buffer);
            } else {
                printf("\r\033[K" BOLD BLUE "[ " RESET GREEN "%s" RESET " | " GREEN "%d" BOLD BLUE " ]: " RESET "%s\n",
                       current_tab, execution_count, buffer);
            }
            add_to_cmd_history(buffer);
            break;
        } else if (c == 127 || c == 8) { // Backspace
            if (pos > 0) {
                for (int j = pos - 1; j < len; j++) buffer[j] = buffer[j+1];
                len--;
                pos--;
                if (history_view_idx == cmd_history_count) {
                    strcpy(temp_buffer, buffer);
                }
            }
        } else if (c == 9) { // Tab
            if (suggestion != NULL) {
                strcpy(buffer, suggestion);
                len = strlen(buffer);
                if (len < max_len - 1) {
                    buffer[len++] = ' ';
                    buffer[len] = '\0';
                }
                pos = len;
            }
        } else if (c == 27) { // Escape sequence
            if (getchar() == '[') {
                char seq = getchar();
                if (seq == 'D') { // Left
                    if (pos > 0) pos--;
                } else if (seq == 'C') { // Right
                    if (pos < len) {
                        pos++;
                    } else if (suggestion != NULL) {
                        // Complete suggestion like Tab
                        strcpy(buffer, suggestion);
                        len = strlen(buffer);
                        if (len < max_len - 1) {
                            buffer[len++] = ' ';
                            buffer[len] = '\0';
                        }
                        pos = len;
                    }
                } else if (seq == 'A') { // Up (History backward)
                    if (history_view_idx > 0) {
                        if (history_view_idx == cmd_history_count) {
                            strcpy(temp_buffer, buffer);
                        }
                        history_view_idx--;
                        strcpy(buffer, cmd_history[history_view_idx]);
                        len = strlen(buffer);
                        pos = len;
                    }
                } else if (seq == 'B') { // Down (History forward)
                    if (history_view_idx < cmd_history_count) {
                        history_view_idx++;
                        if (history_view_idx == cmd_history_count) {
                            strcpy(buffer, temp_buffer);
                        } else {
                            strcpy(buffer, cmd_history[history_view_idx]);
                        }
                        len = strlen(buffer);
                        pos = len;
                    }
                } else if (seq == 'H') { // Home
                    pos = 0;
                } else if (seq == 'F') { // End
                    pos = len;
                } else if (seq == '3') { // Delete key
                    if (getchar() == '~' && pos < len) {
                        for (int j = pos; j < len; j++) buffer[j] = buffer[j+1];
                        len--;
                    }
                }
            }
        } else if (c >= 32 && c <= 126) {
            if (len < max_len - 1) {
                for (int j = len; j > pos; j--) buffer[j] = buffer[j-1];
                buffer[pos] = c;
                len++;
                pos++;
                buffer[len] = '\0';
                if (history_view_idx == cmd_history_count) {
                    strcpy(temp_buffer, buffer);
                }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}


void START() {
    pita = stdin;
    ADV();
}

void ADV() {
    retval = fscanf(pita, "%c", &CC);
    if (retval == EOF) {
        EOP = true;
        CC = MARK;
    } else {
        EOP = (CC == MARK);
    }
}

void IgnoreBlank() {
    while ((CC == BLANK || CC == ENTER) && !EOP) {
        ADV();
    }
}

void STARTKATA() {
    START();
    IgnoreBlank();
    if (EOP) {
        EndKata = true;
    } else {
        EndKata = false;
        SalinKata();
    }
}

void ADVKATA() {
    IgnoreBlank();
    if (EOP) {
        EndKata = true;
    } else {
        SalinKata();
        IgnoreBlank();
    }
}

void STASHKATA() {
    while (!EOP && CC != ENTER) {
        ADV();
    }
    if (!EOP && CC == ENTER) {
        ADV(); // Consume the ENTER
    }
}

void SalinKata() {
    int i = 0;
    while ((CC != MARK) && (CC != BLANK) && (CC != ENTER) && (i < NMax)) {
        CKata.TabKata[i] = CC;
        ADV();
        i++;
    }
    CKata.Length = i;
}

boolean isKataEqual(Kata K1, char* s) {
    int i = 0;
    while (i < K1.Length && s[i] != '\0') {
        if (tolower((unsigned char)K1.TabKata[i]) != tolower((unsigned char)s[i])) {
            return false;
        }
        i++;
    }
    return (i == K1.Length && s[i] == '\0');
}

boolean KataIs(char *s) {
    return isKataEqual(CKata, s);
}
