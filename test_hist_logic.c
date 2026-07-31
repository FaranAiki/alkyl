#include <stdio.h>
#include <string.h>

int main() {
    char cmd_history[10][100] = {
        "let x = 1;",
        "let y = 2;",
        "print(x);",
        "let x = 3;"
    };
    int cmd_history_count = 4;
    int history_view_idx = 4;
    
    char temp_buffer[100] = "let";
    
    // Press UP
    int found_idx = -1;
    int search_len = strlen(temp_buffer);
    for (int i = history_view_idx - 1; i >= 0; i--) {
        if (strncmp(cmd_history[i], temp_buffer, search_len) == 0) {
            found_idx = i;
            break;
        }
    }
    printf("UP 1: %d -> %s\n", found_idx, found_idx >= 0 ? cmd_history[found_idx] : "none");
    history_view_idx = found_idx; // 3
    
    // Press UP again
    found_idx = -1;
    for (int i = history_view_idx - 1; i >= 0; i--) {
        if (strncmp(cmd_history[i], temp_buffer, search_len) == 0) {
            found_idx = i;
            break;
        }
    }
    printf("UP 2: %d -> %s\n", found_idx, found_idx >= 0 ? cmd_history[found_idx] : "none");
    
    return 0;
}
