import sys

with open('src/driver/keyboard.c', 'r') as f:
    content = f.read()

# Replace cursor_up signature and return
content = content.replace('static void cursor_up(char *buffer, int len, int *pos) {', 'static bool cursor_up(char *buffer, int len, int *pos) {')
content = content.replace('    if (current_line_start == 0) return;\n', '    if (current_line_start == 0) return false;\n')
content = content.replace('        *pos = prev_line_start + prev_line_len;\n    }', '        *pos = prev_line_start + prev_line_len;\n    }\n    return true;')

# Replace cursor_down signature and return
content = content.replace('static void cursor_down(char *buffer, int len, int *pos) {', 'static bool cursor_down(char *buffer, int len, int *pos) {')
content = content.replace('    if (next_newline == len) return; // Cursor is already on the last line\n', '    if (next_newline == len) return false; // Cursor is already on the last line\n')
content = content.replace('        *pos = next_line_start + next_line_len;\n    }', '        *pos = next_line_start + next_line_len;\n    }\n    return true;')


target_up = """                } else if (seq2 == 'A') { // Up
                    if (has_unbalanced_braces(input_buffer, len)) {
                        cursor_up(input_buffer, len, &pos);
                    } else {
                        if (history_view_idx > 0) {"""

repl_up = """                } else if (seq2 == 'A') { // Up
                    if (!has_unbalanced_braces(input_buffer, len) || !cursor_up(input_buffer, len, &pos)) {
                        if (history_view_idx > 0) {"""

content = content.replace(target_up, repl_up)

target_down = """                } else if (seq2 == 'B') { // Down
                    if (has_unbalanced_braces(input_buffer, len)) {
                        cursor_down(input_buffer, len, &pos);
                    } else {
                        if (history_view_idx < cmd_history_count) {"""

repl_down = """                } else if (seq2 == 'B') { // Down
                    if (!has_unbalanced_braces(input_buffer, len) || !cursor_down(input_buffer, len, &pos)) {
                        if (history_view_idx < cmd_history_count) {"""

content = content.replace(target_down, repl_down)

with open('src/driver/keyboard.c', 'w') as f:
    f.write(content)

