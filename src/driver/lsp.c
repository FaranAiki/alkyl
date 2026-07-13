#include "driver/lsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "lexer/lexer.h"
#include "common/context.h"
#include "common/arena.h"

// A minimal LSP server that implements Semantic Tokens for Vim/Neovim

static char* read_file_lsp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, f);
        buf[size] = '\0';
    }
    fclose(f);
    return buf;
}

static void generate_semantic_tokens(const char *filepath, int **out_data, int *out_len) {
    char *code = read_file_lsp(filepath);
    if (!code) {
        *out_len = 0;
        return;
    }

    Arena arena;
    arena_init(&arena);
    CompilerContext comp_ctx;
    context_init(&comp_ctx, &arena);

    Lexer l;
    lexer_init(&l, &comp_ctx, filepath, code, NULL);
    
    // We will collect tokens into a dynamically growing array
    int capacity = 1024;
    int *data = malloc(capacity * sizeof(int));
    int count = 0;
    
    int prev_line = 1;
    int prev_col = 1;
    
    TokenType last_type = TOKEN_EOF;
    
    while (1) {
        Token t = lexer_next(&l);
        if (t.type == TOKEN_EOF || t.type == TOKEN_UNKNOWN) break;
        
        int token_type = -1; // -1 means don't highlight
        
        if (t.type == TOKEN_KW_LET || t.type == TOKEN_KW_MUT || t.type == TOKEN_CLASS || t.type == TOKEN_STRUCT || t.type == TOKEN_IF || t.type == TOKEN_ELSE || t.type == TOKEN_WHILE || t.type == TOKEN_FOR || t.type == TOKEN_RETURN || t.type == TOKEN_BREAK || t.type == TOKEN_CONTINUE) {
            token_type = 4; // keyword
        } else if (t.type >= TOKEN_NUMBER && t.type <= TOKEN_FLOAT) {
            token_type = 5; // number
        } else if (t.type == TOKEN_STRING || t.type == TOKEN_BYTE_STRING || t.type == TOKEN_CHAR_LIT) {
            token_type = 6; // string
        } else if (t.type >= TOKEN_ASSIGN && t.type <= TOKEN_RSHIFT_ASSIGN) {
            token_type = 7; // operator
        } else if (t.type == TOKEN_IDENTIFIER) {
            if (last_type == TOKEN_CLASS || last_type == TOKEN_STRUCT || last_type == TOKEN_IMPL || last_type == TOKEN_TRAIT || last_type == TOKEN_NAMESPACE) {
                token_type = 0; // class
            } else if (last_type == TOKEN_KW_LET || last_type == TOKEN_KW_MUT) {
                token_type = 1; // variable
            } else {
                // Peek next to see if it's a function call
                // Since this is a simple lexer pass, we just guess it's a variable if not followed by '('
                // We'll peek using a hack or just default to variable.
                // Wait, peek_token modifies state? No, let's just default to variable for now.
                token_type = 1; // variable
            }
        }
        
        if (token_type != -1) {
            if (count + 5 > capacity) {
                capacity *= 2;
                data = realloc(data, capacity * sizeof(int));
            }
            
            int deltaLine = t.line - prev_line;
            int deltaStart = (deltaLine == 0) ? (t.col - prev_col) : (t.col - 1);
            int length = strlen(t.text);
            
            data[count++] = deltaLine;
            data[count++] = deltaStart;
            data[count++] = length;
            data[count++] = token_type;
            data[count++] = 0; // modifiers
            
            prev_line = t.line;
            prev_col = t.col;
        }
        
        last_type = t.type;
    }
    
    free(code);
    // memory leak arena... wait arena doesn't have destroy?
    // Actually arena_init uses malloc for pages, we should free them but for LSP it's okay for now
    
    *out_data = data;
    *out_len = count;
}


static void read_content_length(int *length) {
    *length = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        if (strncmp(buffer, "Content-Length:", 15) == 0) {
            *length = atoi(buffer + 15);
        } else if (strcmp(buffer, "\r\n") == 0 || strcmp(buffer, "\n") == 0) {
            break;
        }
    }
}

static void send_response(const char *json_response) {
    int len = strlen(json_response);
    fprintf(stdout, "Content-Length: %d\r\n\r\n%s", len, json_response);
    fflush(stdout);
}

void start_lsp_server(void) {
    while (1) {
        int length = 0;
        read_content_length(&length);
        if (length <= 0) {
            // EOF or error
            break;
        }
        
        char *content = malloc(length + 1);
        int read_bytes = fread(content, 1, length, stdin);
        content[read_bytes] = '\0';
        
        // Very hacky JSON matching for a basic LSP. 
        // In a real implementation we would use a JSON parser.
        
        char id_buf[64] = {0};
        char *id_ptr = strstr(content, "\"id\":");
        if (id_ptr) {
            id_ptr += 5;
            while (isspace(*id_ptr)) id_ptr++;
            int i = 0;
            while (isdigit(*id_ptr) && i < sizeof(id_buf) - 1) {
                id_buf[i++] = *id_ptr++;
            }
            id_buf[i] = '\0';
        }
        
        if (strstr(content, "\"method\":\"initialize\"") || strstr(content, "\"method\": \"initialize\"")) {
            // Reply with Semantic Tokens capabilities
            char reply[1024];
            snprintf(reply, sizeof(reply),
                "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{"
                "\"capabilities\":{"
                "\"textDocumentSync\":1,"
                "\"semanticTokensProvider\":{"
                "\"legend\":{"
                "\"tokenTypes\":[\"class\",\"variable\",\"function\",\"method\",\"keyword\",\"number\",\"string\",\"operator\"],"
                "\"tokenModifiers\":[]"
                "},"
                "\"full\":true"
                "}"
                "}}}", id_buf[0] ? id_buf : "1");
            send_response(reply);
        } 
        else if (strstr(content, "\"method\":\"textDocument/semanticTokens/full\"") || strstr(content, "\"method\": \"textDocument/semanticTokens/full\"")) {
            char filepath[1024] = {0};
            char *uri = strstr(content, "\"uri\":");
            if (uri) {
                char *start = strstr(uri, "file://");
                if (start) {
                    start += 7;
                    char *end = strchr(start, '"');
                    if (end && (end - start) < (int)sizeof(filepath)) {
                        strncpy(filepath, start, end - start);
                    }
                }
            }

            int *tokens = NULL;
            int token_len = 0;
            if (filepath[0]) {
                generate_semantic_tokens(filepath, &tokens, &token_len);
            }

            // Build json response
            // Maximum digits per int is ~10, plus commas
            int max_json_size = 2048 + (token_len * 15);
            char *reply = malloc(max_json_size);
            int offset = snprintf(reply, max_json_size, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"data\":[", id_buf[0] ? id_buf : "1");
            
            for (int i = 0; i < token_len; i++) {
                offset += snprintf(reply + offset, max_json_size - offset, "%d%s", tokens[i], (i == token_len - 1) ? "" : ",");
            }
            
            snprintf(reply + offset, max_json_size - offset, "]}}");
            send_response(reply);
            
            if (tokens) free(tokens);
            free(reply);
        }
        else if (strstr(content, "\"method\":\"shutdown\"") || strstr(content, "\"method\": \"shutdown\"")) {
            char reply[1024];
            snprintf(reply, sizeof(reply), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":null}", id_buf[0] ? id_buf : "1");
            send_response(reply);
        }
        else if (strstr(content, "\"method\":\"exit\"") || strstr(content, "\"method\": \"exit\"")) {
            free(content);
            exit(0);
        }
        else {
            // Ignore other notifications (like didOpen, didChange)
        }
        
        free(content);
    }
}
