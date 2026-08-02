#include "parser_internal.h"
#include "expr_pratt.h"
#include <stdlib.h>
#include <string.h>

// Forward declarations
ASTNode* parse_expression_pratt(Parser *p);
static ASTNode* parse_precedence(Parser *p, int precedence);

// Rule table
static ParseRule rules[256]; // Assuming token types don't exceed 256 for now

static ParseRule* get_rule(TokenType type) {
    if (type >= 0 && type < 256) return &rules[type];
    return &rules[0]; // PREC_NONE rule
}

static ASTNode* parse_precedence(Parser *p, int precedence) {
    TokenType type = p->current_token.type;
    ParsePrefixFn prefix_rule = get_rule(type)->prefix;

    if (prefix_rule == NULL) {
        parser_fail(p, "Expected expression");
        return NULL;
    }

    ASTNode *left = prefix_rule(p);

    while (precedence <= get_rule(p->current_token.type)->precedence) {
        type = p->current_token.type;
        ParseInfixFn infix_rule = get_rule(type)->infix;
        if (infix_rule) {
            left = infix_rule(p, left);
        } else {
            break;
        }
    }

    return left;
}

ASTNode* parse_expression_pratt(Parser *p) {
    return parse_precedence(p, PREC_ASSIGN);
}
