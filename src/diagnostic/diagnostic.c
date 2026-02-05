#include "diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// --- Keyword Database for "Did you mean?" ---
static const char* KEYWORDS[] = {
    "loop", "while", "once", "if", "elif", "else", "return", "break", "continue",
    "define", "as", "class", "is", "has", "open", "closed", "typeof",
    "void", "int", "char", "bool", "single", "double", "let",
    "mut", "mutable", "imut", "immutable", "import", "extern", "link",
    "true", "false", "not", NULL
};

// Standard Levenshtein Distance Algorithm
int levenshtein_dist(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    if (abs(len1 - len2) > 3) return abs(len1 - len2);

    int *col = malloc((len1 + 1) * sizeof(int));
    if (!col) return 100;
    
    for (int y = 0; y <= len1; y++) col[y] = y;
    
    for (int x = 1; x <= len2; x++) {
        col[0] = x;
        int last_diag = x - 1;
        for (int y = 1; y <= len1; y++) {
            int old_diag = col[y];
            col[y] = (s1[y-1] == s2[x-1]) ? last_diag : 
                     (1 + (col[y] < col[y-1] ? (col[y] < last_diag ? col[y] : last_diag) : (col[y-1] < last_diag ? col[y-1] : last_diag)));
            last_diag = old_diag;
        }
    }
    
    int result = col[len1];
    free(col);
    return result;
}

const char* find_closest_keyword(const char *ident) {
    if (!ident) return NULL;
    
    const char *best_match = NULL;
    int best_dist = 100;
    int ident_len = strlen(ident);
    
    for (int i = 0; KEYWORDS[i] != NULL; i++) {
        int dist = levenshtein_dist(ident, KEYWORDS[i]);
        
        // Thresholds: strict for short words, lenient for long
        int threshold = (ident_len <= 3) ? 1 : 2;
        
        if (dist <= threshold && dist < best_dist) {
            best_dist = dist;
            best_match = KEYWORDS[i];
        }
    }
    
    return best_match;
}

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "end of file";
        case TOKEN_IDENTIFIER: return "identifier";
        case TOKEN_NUMBER: return "number";
        case TOKEN_FLOAT: return "float";
        case TOKEN_STRING: return "string";
        case TOKEN_CHAR_LIT: return "char";
        
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_COMMA: return ",";
        case TOKEN_DOT: return ".";
        case TOKEN_ELLIPSIS: return "...";
        
        case TOKEN_ASSIGN: return "=";
        case TOKEN_EQ: return "==";
        case TOKEN_NEQ: return "!=";
        case TOKEN_LT: return "<";
        case TOKEN_GT: return ">";
        case TOKEN_LTE: return "<=";
        case TOKEN_GTE: return ">=";
        
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_MOD: return "%";
        
        case TOKEN_INCREMENT: return "++";
        case TOKEN_DECREMENT: return "--";
        
        case TOKEN_AND: return "&";
        case TOKEN_OR: return "|";
        case TOKEN_XOR: return "^";
        case TOKEN_NOT: return "!";
        case TOKEN_BIT_NOT: return "~";
        case TOKEN_AND_AND: return "&&";
        case TOKEN_OR_OR: return "||";
        
        case TOKEN_IF: return "if";
        case TOKEN_ELSE: return "else";
        case TOKEN_WHILE: return "while";
        case TOKEN_LOOP: return "loop";
        case TOKEN_RETURN: return "return";
        case TOKEN_KW_INT: return "int";
        case TOKEN_KW_VOID: return "void";
        case TOKEN_CLASS: return "class";
        case TOKEN_DEFINE: return "define";
        
        // Contextual keywords mapping
        case TOKEN_OPEN: return "open";
        case TOKEN_CLOSED: return "closed";
        case TOKEN_IS: return "is";
        case TOKEN_HAS: return "has";
        
        default: return "token";
    }
}

const char* get_token_description(TokenType type) {
    switch (type) {
        case TOKEN_SEMICOLON: return "semicolon ';'";
        case TOKEN_LBRACE: return "opening brace '{'";
        case TOKEN_RBRACE: return "closing brace '}'";
        case TOKEN_LPAREN: return "opening parenthesis '('";
        case TOKEN_RPAREN: return "closing parenthesis ')'";
        case TOKEN_LBRACKET: return "opening bracket '['";
        case TOKEN_RBRACKET: return "closing bracket ']'";
        case TOKEN_IDENTIFIER: return "identifier";
        default: return token_type_to_string(type);
    }
}

void report_error(Lexer *l, Token t, const char *msg) {
    if (!l || !l->src) {
        fprintf(stderr, DIAG_RED "Error: %s\n" DIAG_RESET, msg);
        return;
    }

    fprintf(stderr, DIAG_BOLD "%d:%d: " DIAG_RED "error: " DIAG_RESET DIAG_BOLD "%s" DIAG_RESET "\n", 
            t.line, t.col, msg);

    const char *line_start = l->src;
    int current_line = 1;
    
    const char *ptr = l->src;
    const char *last_line_ptr = l->src;
    
    while (*ptr && current_line < t.line) {
        if (*ptr == '\n') {
            current_line++;
            last_line_ptr = ptr + 1;
        }
        ptr++;
    }
    line_start = last_line_ptr;

    fprintf(stderr, "  " DIAG_GREY "| " DIAG_RESET); 
    
    const char *c = line_start;
    while (*c && *c != '\n') {
        fputc(*c, stderr);
        c++;
    }
    fputc('\n', stderr);

    fprintf(stderr, "  " DIAG_GREY "| " DIAG_RESET);
    
    c = line_start;
    int col_counter = 1;
    while (col_counter < t.col && *c != '\n' && *c != '\0') {
        if (*c == '\t') fputc('\t', stderr);
        else fputc(' ', stderr);
        c++;
        col_counter++;
    }
    
    fprintf(stderr, DIAG_GREEN "^" DIAG_RESET "\n");
    
    int hint_printed = 0;
    
    if (strstr(msg, "Expected ';'")) {
         fprintf(stderr, DIAG_CYAN "Hint: Try adding a semicolon at the end of the expression.\n" DIAG_RESET);
         hint_printed = 1;
    }
    
    if (t.type == TOKEN_IDENTIFIER && t.text) {
        const char *suggestion = find_closest_keyword(t.text);
        if (suggestion) {
            fprintf(stderr, DIAG_YELLOW "Hint: Did you mean '%s'?\n" DIAG_RESET, suggestion);
            hint_printed = 1;
        }
    }
    
    if (!hint_printed) fprintf(stderr, "\n");
}
