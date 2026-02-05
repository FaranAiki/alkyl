#include "diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "end of file";
        case TOKEN_IDENTIFIER: return "identifier";
        case TOKEN_NUMBER: return "number";
        case TOKEN_FLOAT: return "float";
        case TOKEN_STRING: return "string";
        case TOKEN_CHAR_LIT: return "char";
        
        // Symbols
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
        
        // Operators
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
        
        // Keywords
        case TOKEN_IF: return "if";
        case TOKEN_ELSE: return "else";
        case TOKEN_WHILE: return "while";
        case TOKEN_LOOP: return "loop";
        case TOKEN_RETURN: return "return";
        case TOKEN_VAR_DECL: return "let"; // Map implicit internal type if needed, or keywords directly
        case TOKEN_KW_INT: return "int";
        case TOKEN_KW_VOID: return "void";
        case TOKEN_CLASS: return "class";
        case TOKEN_DEFINE: return "define";
        
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

    // 1. Header
    fprintf(stderr, DIAG_BOLD "%d:%d: " DIAG_RED "error: " DIAG_RESET DIAG_BOLD "%s" DIAG_RESET "\n", 
            t.line, t.col, msg);

    // 2. Find the start of the line in source
    const char *line_start = l->src;
    int current_line = 1;
    
    // Scan from beginning of source to find the line
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

    // 3. Print the code snippet
    // Print line number gutter? Optional, let's keep it simple first
    fprintf(stderr, "  " DIAG_GREY "| " DIAG_RESET); 
    
    const char *c = line_start;
    while (*c && *c != '\n') {
        fputc(*c, stderr);
        c++;
    }
    fputc('\n', stderr);

    // 4. Print the caret pointer
    fprintf(stderr, "  " DIAG_GREY "| " DIAG_RESET);
    
    // We need to iterate again to handle tabs correctly
    c = line_start;
    int col_counter = 1;
    while (col_counter < t.col && *c != '\n' && *c != '\0') {
        if (*c == '\t') fputc('\t', stderr);
        else fputc(' ', stderr);
        c++;
        col_counter++;
    }
    
    fprintf(stderr, DIAG_GREEN "^" DIAG_RESET "\n");
    
    // 5. Optional Hint (Logic can be extended)
    if (strstr(msg, "Expected ';'")) {
         fprintf(stderr, DIAG_CYAN "Hint: Try adding a semicolon at the end of the expression.\n" DIAG_RESET);
    }
    fprintf(stderr, "\n");
}
