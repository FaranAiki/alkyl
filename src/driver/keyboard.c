/**
 * @file keyboard.c
 * @brief Keyboard input handling for the REPL.
 */
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

/**
 * @brief Add a command line to the history buffer.
 * @param line The command line to add.
 */
static void add_to_cmd_history(const char *line) {
    if (strlen(line) == 0) return;
    if (cmd_history_count > 0 && streq_lit(cmd_history[cmd_history_count - 1], line)) return;
    if (cmd_history_count < MAX_HISTORY) {
        snprintf(cmd_history[cmd_history_count], MAX_INPUT_LEN, "%s", line);
        cmd_history_count++;
    } else {
        memmove(cmd_history[0], cmd_history[1], (MAX_HISTORY - 1) * MAX_INPUT_LEN);
        snprintf(cmd_history[MAX_HISTORY - 1], MAX_INPUT_LEN, "%s", line);
    }
}



/**
 * @brief Move the cursor up one line in the buffer.
 * @param buffer The input buffer.
 * @param len Length of the buffer.
 * @param pos Pointer to the current cursor position.
 * @return True if the cursor was moved, false if already at the first line.
 */
static bool cursor_up(char *buffer, int len, int *pos) {
    (void)len;
    int current_line_start = 0;
    for (int i = *pos - 1; i >= 0; i--) {
        if (buffer[i] == '\n') {
            current_line_start = i + 1;
            break;
        }
    }
    if (current_line_start == 0) return false;

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
    return true;
}

/**
 * @brief Move the cursor down one line in the buffer.
 * @param buffer The input buffer.
 * @param len Length of the buffer.
 * @param pos Pointer to the current cursor position.
 * @return True if the cursor was moved, false if already at the last line.
 */
static bool cursor_down(char *buffer, int len, int *pos) {
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

    if (next_newline >= len) return false;

    int next_line_start = next_newline + 1;
    if (next_line_start >= len) return false;

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
    return true;
}

/**
 * @brief Insert a newline with auto-indentation at the current cursor position.
 * @param buffer The input buffer.
 * @param len Pointer to the current buffer length.
 * @param pos Pointer to the current cursor position.
 */
static void insert_newline_line(char *buffer, int *len, int *pos) {
    int line_start = 0;
    for (int i = *pos - 1; i >= 0; i--) {
        if (buffer[i] == '\n') {
            line_start = i + 1;
            break;
        }
    }

    int current_brace = 0;
    for (int i = line_start; i < *pos; i++) {
        if (buffer[i] == '{') current_brace++;
        if (buffer[i] == '}') current_brace--;
    }

    int current_indent = 0;
    for (int i = line_start; i < *pos; i++) {
        if (buffer[i] == ' ' || buffer[i] == '\t') current_indent++;
        else break;
    }

    int word_len = 0;
    int word_start = line_start + current_indent;
    while(word_start + word_len < *pos && isalnum((unsigned char)buffer[word_start + word_len])) word_len++;

    if (current_brace == 0 && word_len > 0) {
        if ((word_len == 2 && memcmp(buffer + word_start, "if", 2) == 0) ||
            (word_len == 3 && memcmp(buffer + word_start, "for", 3) == 0) ||
            (word_len == 5 && memcmp(buffer + word_start, "while", 5) == 0) ||
            (word_len == 4 && memcmp(buffer + word_start, "else", 4) == 0) ||
            (word_len == 4 && memcmp(buffer + word_start, "func", 4) == 0) ||
            (word_len == 5 && memcmp(buffer + word_start, "class", 5) == 0)) {
            // Also ensure it isn't a single-line statement by checking if it ends with a semicolon or brace
            int ends_stmt = 0;
            for (int k = word_start + word_len; k < *pos; k++) {
                if (buffer[k] == ';' || buffer[k] == '}') ends_stmt = 1;
            }
            if (!ends_stmt) {
                current_brace = 1; // fake a brace for auto-indent
            }
        }
    }

    int target_indent = current_indent + current_brace * 4;
    if (target_indent < 0) target_indent = 0;

    int insert_len = 1 + target_indent;
    if (*len + insert_len < MAX_INPUT_LEN - 1) {
        for (int j = *len; j >= *pos; j--) buffer[j + insert_len] = buffer[j];
        buffer[*pos] = '\n';
        for (int j = 1; j <= target_indent; j++) buffer[*pos + j] = ' ';
        *len += insert_len;
        *pos += insert_len;
    }
}

/**
 * @brief Check if the buffer ends with an incomplete operator.
 * @param buffer The input buffer.
 * @param len Length of the buffer.
 * @return 1 if the buffer ends with an incomplete operator, 0 otherwise.
 */
static int ends_with_incomplete_operator(const char *buffer, int len) {
    while (len > 0 && (buffer[len-1] == ' ' || buffer[len-1] == '\t')) len--;
    if (len == 0) return 0;

    if (len >= 2) {
        int as_end = 0;
        if (buffer[len-2] == 'a' && buffer[len-1] == 's') {
            if (len == 2 || (!isalnum((unsigned char)buffer[len-3]) && buffer[len-3] != '_')) {
                as_end = 1;
            }
        }
        if (as_end) return 1;
    }

    if (len >= 2) {
        char a = buffer[len-2], b = buffer[len-1];
        if ((a == '+' && b == '=') || (a == '-' && b == '=') || (a == '*' && b == '=') ||
            (a == '/' && b == '=') || (a == '%' && b == '=') || (a == '&' && b == '=') ||
            (a == '|' && b == '=') || (a == '^' && b == '=') || (a == '<' && b == '=') ||
            (a == '>' && b == '=') || (a == '=' && b == '=') || (a == '!' && b == '=') ||
            (a == '<' && b == '<') || (a == '>' && b == '>') || (a == '&' && b == '&') ||
            (a == '|' && b == '|') || (a == ':' && b == '=') || (a == '?' && b == '?')) {
            return 1;
        }
    }

    char last = buffer[len-1];
    if (last == '=' || last == '+' || last == '-' || last == '*' || last == '/' ||
        last == '%' || last == '&' || last == '|' || last == '^' || last == '<' ||
        last == '>' || last == '?' || last == ':' || last == '!' || last == '~') {
        if (len >= 2 && buffer[len-2] == last && (last == '+' || last == '-')) {
            return 0;
        }
        return 1;
    }

    return 0;
}

/**
 * @brief Check if the current line needs continuation (e.g., after if/for/while).
 * @param buffer The input buffer.
 * @param len Length of the buffer.
 * @param indentation_scope Whether indentation-based scope is active.
 * @return 1 if continuation is needed, 0 otherwise.
 */
static int needs_continuation(const char *buffer, int len, int indentation_scope) {
    int i = len - 1;
    while (i >= 0 && (buffer[i] == ' ' || buffer[i] == '\t' || buffer[i] == '\n' || buffer[i] == '\r')) i--;
    if (i < 0) return 0;
    if (buffer[i] == '{' || buffer[i] == ';' || buffer[i] == '}') return 0;

    int line_start = i;
    while (line_start > 0 && buffer[line_start - 1] != '\n') {
        line_start--;
    }
    while (line_start <= i && (buffer[line_start] == ' ' || buffer[line_start] == '\t')) {
        line_start++;
    }

    int word_len = 0;
    while (line_start + word_len <= i && isalnum((unsigned char)buffer[line_start + word_len])) {
        word_len++;
    }

    if (word_len > 0) {
        if (word_len == 2 && memcmp(buffer + line_start, "if", 2) == 0) return 1;
        if (word_len == 3 && memcmp(buffer + line_start, "for", 3) == 0) return 1;
        if (word_len == 5 && memcmp(buffer + line_start, "while", 5) == 0) return 1;
        if (word_len == 4 && memcmp(buffer + line_start, "else", 4) == 0) return 1;
        if (word_len == 4 && memcmp(buffer + line_start, "func", 4) == 0) return 1;
        if (word_len == 5 && memcmp(buffer + line_start, "class", 5) == 0) return 1;
    }

    if (!indentation_scope) return 0;

    // Check if the current line has indentation. If it does, we are in a block,
    // so we need continuation, UNLESS the line is entirely whitespace.
    int current_indent = 0;
    int k = line_start;
    while (k <= i && (buffer[k] == ' ' || buffer[k] == '\t')) {
        current_indent++;
        k++;
    }

    // If the line is not just whitespace and it's indented, continue block
    if (current_indent > 0 && k <= i) {
        return 1;
    }

    return 0;
}

/**
 * @brief Get the indentation of a line.
 * @param buffer The input buffer.
 * @param line_start Start index of the line.
 * @param end End index of the line.
 * @return Number of leading spaces/tabs.
 */
static int get_line_indent(const char *buffer, int line_start, int end) {
    int indent = 0;
    for (int i = line_start; i < end; i++) {
        if (buffer[i] == ' ' || buffer[i] == '\t') indent++;
        else break;
    }
    return indent;
}

/**
 * @brief Get the base (minimum) indentation across all non-empty lines.
 * @param buffer The input buffer.
 * @param len Length of the buffer.
 * @return Base indentation level.
 */
static int get_base_indent(const char *buffer, int len) {
    int base_indent = -1;
    int line_start = 0;
    for (int i = 0; i <= len; i++) {
        if (i == len || buffer[i] == '\n') {
            int indent = get_line_indent(buffer, line_start, i);
            if (i > line_start || indent > 0) {
                if (base_indent < 0 || indent < base_indent) base_indent = indent;
            }
            line_start = i + 1;
        }
    }
    return base_indent >= 0 ? base_indent : 0;
}

/**
 * @brief Move the cursor to the beginning of the previous word.
 * @param buffer The input buffer.
 * @param len Length of the buffer.
 * @param pos Pointer to the current cursor position.
 */
static void cursor_word_left(char *buffer, int len, int *pos) {
    (void)len;
    if (*pos == 0) return;
    int i = *pos - 1;
    while (i >= 0 && !isalnum((unsigned char)buffer[i]) && buffer[i] != '_') i--;
    while (i >= 0 && (isalnum((unsigned char)buffer[i]) || buffer[i] == '_')) i--;
    *pos = i + 1;
}

/**
 * @brief Move the cursor to the beginning of the next word.
 * @param buffer The input buffer.
 * @param len Length of the buffer.
 * @param pos Pointer to the current cursor position.
 */
static void cursor_word_right(char *buffer, int len, int *pos) {
    if (*pos >= len) return;
    int i = *pos;
    while (i < len && !isalnum((unsigned char)buffer[i]) && buffer[i] != '_') i++;
    while (i < len && (isalnum((unsigned char)buffer[i]) || buffer[i] == '_')) i++;
    *pos = i;
}

/**
 * @brief Redraw the REPL prompt and input buffer with syntax highlighting.
 * @param base_prompt Colored prompt string.
 * @param base_prompt_no_color Uncolored prompt string for width calculations.
 * @param buffer The input buffer.
 * @param len Length of the buffer.
 * @param pos Current cursor position.
 * @param suggestion Current autocomplete suggestion.
 * @param word_len_sugg Length of the suggestion prefix.
 * @param last_cursor_row Pointer to the last rendered cursor row.
 * @param sem_ctx Semantic context for symbol highlighting.
 */
static void redraw(const char *base_prompt, const char *base_prompt_no_color, const char *buffer, int len, int pos, const char *suggestion, int word_len_sugg, int *last_cursor_row, void *sem_ctx) {
    if (*last_cursor_row > 0) {
        char seq[32];
        snprintf(seq, sizeof(seq), "\033[%dA", *last_cursor_row);
        printf("%s", seq);
    }
    printf("\r" CLEAR_LINE CLEAR_SCREEN);

    int base_prompt_len = strlen(base_prompt_no_color);

    int target_row = 0;
    int target_col = 0;

    int line_start = 0;
    int current_row = 0;
    int hl_state = 0;

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

            const char *cur_color = NULL;
            int j = line_start;
            while (j < i) {
                const char *color = NULL;
                char c = buffer[j];
                int token_end = j;

                if (hl_state == 1) {
                    color = COLOR_GRAY;
                    if (c == '\n') hl_state = 0;
                } else if (hl_state == 2) {
                    color = COLOR_GREEN;
                    if (c == '"') hl_state = 0;
                    else if (c == '\\' && j + 1 < i) token_end = j + 1;
                } else if (hl_state == 3) {
                    color = COLOR_GREEN;
                    if (c == '\'') hl_state = 0;
                    else if (c == '\\' && j + 1 < i) token_end = j + 1;
                } else if (hl_state == 4) {
                    color = COLOR_GRAY;
                    if (c == '*' && j + 1 < i && buffer[j+1] == '/') {
                        hl_state = 0;
                        token_end = j + 1;
                    }
                } else {
                    if (c == '/' && j + 1 < i && buffer[j+1] == '/') {
                        color = COLOR_GRAY;
                        hl_state = 1;
                    } else if (c == '/' && j + 1 < i && buffer[j+1] == '*') {
                        color = COLOR_GRAY;
                        hl_state = 4;
                    } else if (c == '"') {
                        color = COLOR_GREEN;
                        hl_state = 2;
                    } else if (c == '\'') {
                        color = COLOR_GREEN;
                        hl_state = 3;
                    } else if (c >= '0' && c <= '9') {
                        int num_start = j;
                        token_end = j;
                        while (token_end + 1 < i && ((buffer[token_end+1] >= '0' && buffer[token_end+1] <= '9') || buffer[token_end+1] == '.' || buffer[token_end+1] == 'x' || buffer[token_end+1] == 'X' || buffer[token_end+1] == 'b' || buffer[token_end+1] == 'B' || buffer[token_end+1] == 'e' || buffer[token_end+1] == 'E' || buffer[token_end+1] == '+' || buffer[token_end+1] == '-')) token_end++;
                        if (token_end + 1 < i && ((buffer[token_end+1] >= 'a' && buffer[token_end+1] <= 'f') || (buffer[token_end+1] >= 'A' && buffer[token_end+1] <= 'F'))) {
                            token_end = num_start;
                        } else if (token_end + 1 < i && (isalpha((unsigned char)buffer[token_end+1]) || buffer[token_end+1] == '_')) {
                            token_end = num_start;
                        } else {
                            color = COLOR_MAGENTA;
                        }
                    } else if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                        int word_start = j;
                        token_end = j;
                        while (token_end + 1 < i && ((buffer[token_end+1] >= 'a' && buffer[token_end+1] <= 'z') || (buffer[token_end+1] >= 'A' && buffer[token_end+1] <= 'Z') || (buffer[token_end+1] >= '0' && buffer[token_end+1] <= '9') || buffer[token_end+1] == '_')) token_end++;
                        int word_len = token_end - word_start + 1;
                        const char *kw = NULL;
                        // TODO: whatever the fuck is this, make this modular
                        // Declarations
                        if (word_len == 3 && memcmp(buffer + word_start, "let", 3) == 0) kw = COLOR_BLUE;
                        else if (word_len == 5 && memcmp(buffer + word_start, "class", 5) == 0) kw = COLOR_BLUE;
                        else if (word_len == 6 && memcmp(buffer + word_start, "struct", 6) == 0) kw = COLOR_BLUE;
                        else if (word_len == 5 && memcmp(buffer + word_start, "union", 5) == 0) kw = COLOR_BLUE;
                        else if (word_len == 4 && memcmp(buffer + word_start, "enum", 4) == 0) kw = COLOR_BLUE;
                        else if (word_len == 6 && memcmp(buffer + word_start, "errnum", 6) == 0) kw = COLOR_BLUE;
                        else if (word_len == 6 && memcmp(buffer + word_start, "import", 6) == 0) kw = COLOR_BLUE;
                        else if (word_len == 6 && memcmp(buffer + word_start, "export", 6) == 0) kw = COLOR_BLUE;
                        else if (word_len == 9 && memcmp(buffer + word_start, "namespace", 9) == 0) kw = COLOR_BLUE;
                        else if (word_len == 7 && memcmp(buffer + word_start, "typedef", 7) == 0) kw = COLOR_BLUE;
                        else if (word_len == 6 && memcmp(buffer + word_start, "define", 6) == 0) kw = COLOR_BLUE;
                        // Modifiers
                        else if (word_len == 3 && memcmp(buffer + word_start, "mut", 3) == 0) kw = COLOR_RED;
                        else if (word_len == 4 && memcmp(buffer + word_start, "imut", 4) == 0) kw = COLOR_RED;
                        else if (word_len == 6 && memcmp(buffer + word_start, "extern", 6) == 0) kw = COLOR_RED;
                        else if (word_len == 4 && memcmp(buffer + word_start, "pure", 4) == 0) kw = COLOR_RED;
                        else if (word_len == 6 && memcmp(buffer + word_start, "impure", 6) == 0) kw = COLOR_RED;
                        else if (word_len == 8 && memcmp(buffer + word_start, "pristine", 8) == 0) kw = COLOR_RED;
                        else if (word_len == 7 && memcmp(buffer + word_start, "tainted", 7) == 0) kw = COLOR_RED;
                        else if (word_len == 8 && memcmp(buffer + word_start, "covalent", 8) == 0) kw = COLOR_RED;
                        else if (word_len == 4 && memcmp(buffer + word_start, "meta", 4) == 0) kw = COLOR_RED;
                        else if (word_len == 6 && memcmp(buffer + word_start, "public", 6) == 0) kw = COLOR_RED;
                        else if (word_len == 7 && memcmp(buffer + word_start, "private", 7) == 0) kw = COLOR_RED;
                        else if (word_len == 4 && memcmp(buffer + word_start, "open", 4) == 0) kw = COLOR_RED;
                        else if (word_len == 6 && memcmp(buffer + word_start, "closed", 6) == 0) kw = COLOR_RED;
                        else if (word_len == 5 && memcmp(buffer + word_start, "const", 5) == 0) kw = COLOR_RED;
                        else if (word_len == 5 && memcmp(buffer + word_start, "inert", 5) == 0) kw = COLOR_RED;
                        else if (word_len == 7 && memcmp(buffer + word_start, "reactive", 7) == 0) kw = COLOR_RED;
                        else if (word_len == 5 && memcmp(buffer + word_start, "naked", 5) == 0) kw = COLOR_RED;
                        else if (word_len == 6 && memcmp(buffer + word_start, "static", 6) == 0) kw = COLOR_RED;
                        else if (word_len == 8 && memcmp(buffer + word_start, "abstract", 8) == 0) kw = COLOR_RED;
                        else if (word_len == 5 && memcmp(buffer + word_start, "exact", 5) == 0) kw = COLOR_RED;
                        else if (word_len == 5 && memcmp(buffer + word_start, "pragma", 5) == 0) kw = COLOR_RED;
                        else if (word_len == 6 && memcmp(buffer + word_start, "method", 6) == 0) kw = COLOR_RED;
                        else if (word_len == 9 && memcmp(buffer + word_start, "container", 9) == 0) kw = COLOR_RED;
                        else if (word_len == 5 && memcmp(buffer + word_start, "frame", 5) == 0) kw = COLOR_RED;
                        else if (word_len == 5 && memcmp(buffer + word_start, "total", 5) == 0) kw = COLOR_RED;
                        else if (word_len == 7 && memcmp(buffer + word_start, "partial", 7) == 0) kw = COLOR_RED;
                        else if (word_len == 8 && memcmp(buffer + word_start, "extended", 8) == 0) kw = COLOR_RED;
                        else if (word_len == 8 && memcmp(buffer + word_start, "override", 8) == 0) kw = COLOR_RED;
                        // Control flow
                        else if (word_len == 2 && memcmp(buffer + word_start, "if", 2) == 0) kw = COLOR_CYAN;
                        else if (word_len == 4 && memcmp(buffer + word_start, "else", 4) == 0) kw = COLOR_CYAN;
                        else if (word_len == 4 && memcmp(buffer + word_start, "then", 4) == 0) kw = COLOR_CYAN;
                        else if (word_len == 5 && memcmp(buffer + word_start, "while", 5) == 0) kw = COLOR_CYAN;
                        else if (word_len == 3 && memcmp(buffer + word_start, "for", 3) == 0) kw = COLOR_CYAN;
                        else if (word_len == 2 && memcmp(buffer + word_start, "in", 2) == 0) kw = COLOR_CYAN;
                        else if (word_len == 6 && memcmp(buffer + word_start, "return", 6) == 0) kw = COLOR_CYAN;
                        else if (word_len == 6 && memcmp(buffer + word_start, "switch", 6) == 0) kw = COLOR_CYAN;
                        else if (word_len == 4 && memcmp(buffer + word_start, "case", 4) == 0) kw = COLOR_CYAN;
                        else if (word_len == 5 && memcmp(buffer + word_start, "break", 5) == 0) kw = COLOR_CYAN;
                        else if (word_len == 8 && memcmp(buffer + word_start, "continue", 8) == 0) kw = COLOR_CYAN;
                        else if (word_len == 2 && memcmp(buffer + word_start, "as", 2) == 0) kw = COLOR_CYAN;
                        // Literals
                        else if (word_len == 4 && memcmp(buffer + word_start, "true", 4) == 0) kw = COLOR_MAGENTA;
                        else if (word_len == 5 && memcmp(buffer + word_start, "false", 5) == 0) kw = COLOR_MAGENTA;
                        else if (word_len == 4 && memcmp(buffer + word_start, "null", 4) == 0) kw = COLOR_MAGENTA;
                        // Other keywords
                        else if (word_len == 5 && memcmp(buffer + word_start, "purge", 5) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 8 && memcmp(buffer + word_start, "compound", 8) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 4 && memcmp(buffer + word_start, "flux", 4) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 4 && memcmp(buffer + word_start, "emit", 4) == 0) kw = COLOR_YELLOW;
                        // Built-ins
                        else if (word_len == 6 && memcmp(buffer + word_start, "typeof", 6) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 9 && memcmp(buffer + word_start, "hasmethod", 9) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 11 && memcmp(buffer + word_start, "hasattribute", 11) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 6 && memcmp(buffer + word_start, "sizeof", 6) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 7 && memcmp(buffer + word_start, "alignof", 7) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 7 && memcmp(buffer + word_start, "defined", 7) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 11 && memcmp(buffer + word_start, "iscompatible", 11) == 0) kw = COLOR_YELLOW;
                        // Types
                        else if (word_len == 3 && memcmp(buffer + word_start, "int", 3) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 5 && memcmp(buffer + word_start, "float", 5) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 6 && memcmp(buffer + word_start, "double", 6) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 6 && memcmp(buffer + word_start, "single", 6) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 4 && memcmp(buffer + word_start, "bool", 4) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 4 && memcmp(buffer + word_start, "void", 4) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 8 && memcmp(buffer + word_start, "noreturn", 8) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 4 && memcmp(buffer + word_start, "char", 4) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 6 && memcmp(buffer + word_start, "string", 6) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 5 && memcmp(buffer + word_start, "short", 5) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 4 && memcmp(buffer + word_start, "long", 4) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 8 && memcmp(buffer + word_start, "unsigned", 8) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 6 && memcmp(buffer + word_start, "signed", 6) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 5 && memcmp(buffer + word_start, "usize", 5) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 6 && memcmp(buffer + word_start, "size_t", 6) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 2 && memcmp(buffer + word_start, "i8", 2) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 3 && memcmp(buffer + word_start, "i16", 3) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 3 && memcmp(buffer + word_start, "i32", 3) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 3 && memcmp(buffer + word_start, "i64", 3) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 2 && memcmp(buffer + word_start, "u8", 2) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 3 && memcmp(buffer + word_start, "u16", 3) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 3 && memcmp(buffer + word_start, "u32", 3) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 3 && memcmp(buffer + word_start, "u64", 3) == 0) kw = COLOR_YELLOW;
                        else if (word_len == 7 && memcmp(buffer + word_start, "mutable", 7) == 0) kw = COLOR_CYAN;
                        else if (word_len == 9 && memcmp(buffer + word_start, "immutable", 9) == 0) kw = COLOR_CYAN;

                        if (!kw && sem_ctx) {
                            char word_buf[256];
                            int wl = word_len < 255 ? word_len : 255;
                            memcpy(word_buf, buffer + word_start, wl);
                            word_buf[wl] = '\0';

                            SemanticCtx *sem = (SemanticCtx*)sem_ctx;
                            SemSymbol *sym = sem_symbol_lookup(sem, word_buf, NULL);
                            if (sym) {
                                if (sym->kind == SYM_CLASS || sym->kind == SYM_ENUM) kw = COLOR_YELLOW;
                                else if (sym->kind == SYM_FUNC || sym->kind == SYM_TEMPLATE) kw = COLOR_BLUE;
                                else if (sym->kind == SYM_NAMESPACE) kw = COLOR_GREEN;
                                else if (sym->kind == SYM_VAR) kw = COLOR_CYAN;
                            }
                        }
                        color = kw;
                    }
                }

                if (color != cur_color) {
                    if (cur_color) printf(COLOR_RESET);
                    if (color) printf("%s", color);
                    cur_color = color;
                }

                for (int k = j; k <= token_end; k++) {
                    putchar(buffer[k]);
                    if (k == pos) target_col = k - line_start;
                }
                j = token_end + 1;
            }
            if (cur_color) printf(COLOR_RESET);

            if (suggestion != NULL && pos == len && i == len) {
                printf(COLOR_GRAY "%s" COLOR_RESET, suggestion + word_len_sugg);
            }

            if (i < len) {
                printf("\n");
            }

            hl_state = 0;
            line_start = i + 1;
            current_row++;
        }
    }

    int total_rows = current_row;

    int rows_up = (total_rows - 1) - target_row;
    if (rows_up > 0) {
        char seq[32];
        snprintf(seq, sizeof(seq), "\033[%dA", rows_up);
        printf("%s", seq);
    }

    printf("\r");
    if (base_prompt_len + target_col > 0) {
        char seq[32];
        snprintf(seq, sizeof(seq), "\033[%dC", base_prompt_len + target_col);
        printf("%s", seq);
    }

    *last_cursor_row = target_row;
    fflush(stdout);
}

/**
 * @brief Read a line of input from a piped stdin (non-interactive mode).
 * @param arena Arena allocator for the input buffer.
 * @param cmd_count Current command count for the prompt.
 * @return The input string, or NULL on EOF/error.
 */
static char* get_smart_input_piped(void *arena, int cmd_count) {
    char prompt[128];
    snprintf(prompt, sizeof(prompt), PROMPT_COLOR "In [%d]:" PROMPT_RESET " ", cmd_count);

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

/**
 * @brief Read interactive input with smart editing features.
 * @param arena Arena allocator for the input buffer.
 * @param cmd_count Current command count for the prompt.
 * @param sem_ctx Semantic context for autocomplete.
 * @param indentation_scope Whether indentation-based scope is active.
 * @return The input string, or NULL on EOF/error.
 */
char* get_smart_input(void *arena, int cmd_count, void *sem_ctx, int indentation_scope) {
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
    snprintf(base_prompt, sizeof(base_prompt), PROMPT_COLOR "In [%d]:" PROMPT_RESET " ", cmd_count);
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
        SymbolKind suggested_kind = SYM_VAR;
        int word_start = current_line_start;

        if (pos == len) {
            for (int i = pos - 1; i >= current_line_start; i--) {
                if (!isalnum(input_buffer[i]) && input_buffer[i] != '_') {
                    word_start = i + 1;
                    break;
                }
            }
            word_len = pos - word_start;
            if (word_len > 0 || (word_start > 0 && input_buffer[word_start - 1] == '.')) {
                if (sem_ctx != NULL) {
                    SemanticCtx *sem = (SemanticCtx*)sem_ctx;
                    SemScope *scope = sem->current_scope;
                    int after_dot = 0;

                    if (word_start > 0 && input_buffer[word_start - 1] == '.') {
                        after_dot = 1;
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
                                suggested_kind = sym->kind;
                                break;
                            }
                            sym = sym->next;
                        }
                        if (suggestion != NULL) break;
                        if (after_dot) break; // only search the exact namespace/class
                        scope = scope->parent;
                    }
                }

                if (suggestion == NULL && !(word_start > 0 && input_buffer[word_start - 1] == '.')) {
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

        redraw(base_prompt, base_prompt_no_color, input_buffer, len, pos, suggestion, word_len, &last_cursor_row, sem_ctx);

        int c = getchar();

        if (c == EOF || c == 4) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            printf("\n");
            return NULL;
        } else if (c == 3) {
            len = 0;
            pos = 0;
            input_buffer[0] = '\0';
            continue;
        }

        if (c == '\n' || c == '\r') {
            if (len == 0 && c == '\n') continue;

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

            int current_line_start = 0;
            for (int i = pos - 1; i >= 0; i--) {
                if (input_buffer[i] == '\n') {
                    current_line_start = i + 1;
                    break;
                }
            }
            int current_indent = get_line_indent(input_buffer, current_line_start, pos);
            int base_indent = get_base_indent(input_buffer, len);
            int current_line_empty = (pos <= current_line_start) ||
                (pos > current_line_start &&
                 get_line_indent(input_buffer, current_line_start, pos) >= (pos - current_line_start));

            int cont = ends_with_incomplete_operator(input_buffer, len);
            if (!cont) cont = needs_continuation(input_buffer, len, indentation_scope);

            if (paren <= 0 && bracket <= 0 && brace <= 0 && !in_str && !in_char &&
                !cont && (current_line_empty || current_indent <= base_indent)) {
                redraw(base_prompt, base_prompt_no_color, input_buffer, len, pos, NULL, 0, &last_cursor_row, sem_ctx);

                int total_rows = 1;
                for(int i = 0; i < len; i++) if (input_buffer[i] == '\n') total_rows++;
                int rows_down = (total_rows - 1) - last_cursor_row;
                if (rows_down > 0) {
                    char seq[32];
                    snprintf(seq, sizeof(seq), "\033[%dB", rows_down);
                    printf("%s", seq);
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
                size_t sug_len = strlen(suggestion);
                size_t remaining = MAX_INPUT_LEN - word_start - 1;
                if (sug_len > remaining) sug_len = remaining;
                memcpy(input_buffer + word_start, suggestion, sug_len);
                input_buffer[word_start + sug_len] = '\0';
                int added_len = (int)(sug_len - word_len);
                if (len + added_len + 1 < MAX_INPUT_LEN - 1) {
                    char append_char = ' ';
                    if (suggested_kind == SYM_NAMESPACE || suggested_kind == SYM_CLASS) {
                        append_char = '.';
                    }
                    for(int j = len; j >= pos; j--) input_buffer[j + added_len + 1] = input_buffer[j];
                    input_buffer[pos + added_len] = append_char;
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
                        size_t sug_len = strlen(suggestion);
                        size_t remaining = MAX_INPUT_LEN - word_start - 1;
                        if (sug_len > remaining) sug_len = remaining;
                        memcpy(input_buffer + word_start, suggestion, sug_len);
                        input_buffer[word_start + sug_len] = '\0';
                        int added_len = (int)(sug_len - word_len);
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
                    if (!cursor_up(input_buffer, len, &pos)) {
                        if (history_view_idx > 0) {
                            if (history_view_idx == cmd_history_count) {
                                strcpy(temp_buffer, input_buffer);
                            }
                            int found_idx = -1;
                            int search_len = strlen(temp_buffer);
                            if (search_len == 0) {
                                found_idx = history_view_idx - 1;
                            } else {
                                for (int i = history_view_idx - 1; i >= 0; i--) {
                                    if (strncmp(cmd_history[i], temp_buffer, search_len) == 0) {
                                        found_idx = i;
                                        break;
                                    }
                                }
                            }
                            if (found_idx >= 0) {
                                history_view_idx = found_idx;
                                strcpy(input_buffer, cmd_history[history_view_idx]);
                                len = strlen(input_buffer);
                                pos = len;
                            }
                        }
                    }
                } else if (seq2 == 'B') { // Down
                    if (!cursor_down(input_buffer, len, &pos)) {
                        if (history_view_idx < cmd_history_count) {
                            int found_idx = cmd_history_count;
                            int search_len = strlen(temp_buffer);
                            if (search_len == 0) {
                                found_idx = history_view_idx + 1;
                            } else {
                                for (int i = history_view_idx + 1; i < cmd_history_count; i++) {
                                    if (strncmp(cmd_history[i], temp_buffer, search_len) == 0) {
                                        found_idx = i;
                                        break;
                                    }
                                }
                            }

                            history_view_idx = found_idx;
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
        } else if (c >= 32 && c != 127) {
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

    return NULL;
}
