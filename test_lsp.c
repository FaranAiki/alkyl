#include <stdio.h>
#include <string.h>

int main() {
    char req[] = "\"uri\":\"file:///path/to/test.aky\"";
    char *uri = strstr(req, "\"uri\"");
    if (uri) {
        char *start = strstr(uri, "file://");
        if (start) {
            start += 7;
            char *end = strchr(start, '"');
            if (end) {
                *end = '\0';
                printf("Path: %s\n", start);
            }
        }
    }
    return 0;
}
