#include "parser.h"
#include <string.h>

int parse_modifiers(Parser* p) {
    int modifiers = 0;
    // TODO switch this to switch
    while (1) {
        if (p->current_token.type == TOKEN_PUBLIC) {
            modifiers |= MODIFIER_PUBLIC;
            eat(p, TOKEN_PUBLIC);
        } else if (p->current_token.type == TOKEN_PRIVATE) {
            modifiers |= MODIFIER_PRIVATE;
            eat(p, TOKEN_PRIVATE);
        } else if (p->current_token.type == TOKEN_OPEN) {
            modifiers |= MODIFIER_OPEN;
            eat(p, TOKEN_OPEN);
        } else if (p->current_token.type == TOKEN_CLOSED) {
            modifiers |= MODIFIER_CLOSED;
            eat(p, TOKEN_CLOSED);
        } else if (p->current_token.type == TOKEN_CONST) {
            modifiers |= MODIFIER_CONST;
            eat(p, TOKEN_CONST);
        } else if (p->current_token.type == TOKEN_FINAL) {
            modifiers |= MODIFIER_FINAL;
            eat(p, TOKEN_FINAL);
        } else if (p->current_token.type == TOKEN_INERT) {
            modifiers |= MODIFIER_INERT;
            eat(p, TOKEN_INERT);
        } else if (p->current_token.type == TOKEN_REACTIVE) {
            modifiers |= MODIFIER_REACTIVE;
            eat(p, TOKEN_REACTIVE);
        } else if (p->current_token.type == TOKEN_NAKED) {
            modifiers |= MODIFIER_NAKED;
            eat(p, TOKEN_NAKED);
        } else if (p->current_token.type == TOKEN_PURE) {
            modifiers |= MODIFIER_PURE;
            eat(p, TOKEN_PURE);
        } else if (p->current_token.type == TOKEN_IMPURE) {
            modifiers |= MODIFIER_IMPURE;
            eat(p, TOKEN_IMPURE);
        } else if (p->current_token.type == TOKEN_PRISTINE) {
            modifiers |= MODIFIER_PRISTINE;
            eat(p, TOKEN_PRISTINE);
        } else if (p->current_token.type == TOKEN_TAINTED) {
            modifiers |= MODIFIER_TAINTED;
            eat(p, TOKEN_TAINTED);
        } else if (p->current_token.type == TOKEN_COVALENT) {
            modifiers |= MODIFIER_COVALENT;
            eat(p, TOKEN_COVALENT);
        } else if (p->current_token.type == TOKEN_IDENTIFIER && strcmp(p->current_token.text, "static") == 0) {
            modifiers |= MODIFIER_STATIC;
            eat(p, TOKEN_IDENTIFIER);
        } else {
            break; // No more modifiers found
        }
    }
    return modifiers;
}

// Applies extracted modifiers correctly to ClassNodes
void apply_class_modifiers(ClassNode* node, int modifiers) {
    if (modifiers & MODIFIER_PUBLIC) node->is_public = 1;
    if (modifiers & MODIFIER_PRIVATE) node->is_public = 0;
    if (modifiers & MODIFIER_OPEN) node->is_open = 1;
    if (modifiers & MODIFIER_CLOSED) node->is_open = 0;
    if (modifiers & MODIFIER_STATIC) node->is_static = 1;
    
    node->is_pure = !(modifiers & MODIFIER_IMPURE);
    node->has_explicit_pure = (modifiers & MODIFIER_PURE) != 0;
    
    // IS-A constraints (Inheritance)
    if (modifiers & MODIFIER_FINAL) {
        node->is_is_a = IS_A_FINAL;
    } else if (modifiers & MODIFIER_NAKED) {
        node->is_is_a = IS_A_NAKED;
    } else {
        node->is_is_a = IS_A_NONE;
    }

    // HAS-A constraints (Composition)
    if (modifiers & MODIFIER_INERT) {
        node->is_has_a = HAS_A_INERT;
    } else if (modifiers & MODIFIER_REACTIVE) {
        node->is_has_a = HAS_A_REACTIVE;
    } else {
        node->is_has_a = HAS_A_NONE;
    }
}

// Applies extracted modifiers correctly to FuncDefNodes
void apply_func_modifiers(FuncDefNode* node, int modifiers) {
    if (modifiers & MODIFIER_PUBLIC) node->is_public = 1;
    if (modifiers & MODIFIER_PRIVATE) node->is_public = 0;
    if (modifiers & MODIFIER_OPEN) node->is_open = 1;
    if (modifiers & MODIFIER_CLOSED) node->is_open = 0;
    if (modifiers & MODIFIER_STATIC) node->is_static = 1;
    if (modifiers & MODIFIER_COVALENT) node->is_covalent = 1;
    
    // node is pure is TRUE by default unless proven otherwise (e.g. explicitly impure)
    node->is_pure = !(modifiers & MODIFIER_IMPURE);
    node->has_explicit_pure = (modifiers & MODIFIER_PURE) != 0;
    
    // Extern functions are tainted by default unless explicitly marked pristine
    if (node->is_extern) {
        node->is_pristine = (modifiers & MODIFIER_PRISTINE) != 0;
    } else {
        node->is_pristine = !(modifiers & MODIFIER_TAINTED);
    }
    
    if (!node->is_pristine) {
        node->ret_type.is_tainted = 1;
    } else if (node->ret_type.is_tainted) {
        node->is_pristine = 0;
    }
    node->has_explicit_pristine = (modifiers & MODIFIER_PRISTINE) != 0;

    // oop modifier useless af 
    if (modifiers & MODIFIER_FINAL) {
        node->is_is_a = IS_A_FINAL;
    } else if (modifiers & MODIFIER_NAKED) {
        node->is_is_a = IS_A_NAKED;
    } else {
        node->is_is_a = IS_A_NONE;
    }

    if (modifiers & MODIFIER_INERT) {
        node->is_has_a = HAS_A_INERT;
    } else if (modifiers & MODIFIER_REACTIVE) {
        node->is_has_a = HAS_A_REACTIVE;
    } else {
        node->is_has_a = HAS_A_NONE;
    }
}

// Applies extracted modifiers correctly to VarDeclNodes
void apply_var_modifiers(VarDeclNode* node, int modifiers) {
    if (modifiers & MODIFIER_PUBLIC) node->is_public = 1;
    if (modifiers & MODIFIER_PRIVATE) node->is_public = 0;
    if (modifiers & MODIFIER_OPEN) node->is_open = 1;
    if (modifiers & MODIFIER_CLOSED) node->is_open = 0;
    
    // Core variable properties
    node->is_const = (modifiers & MODIFIER_CONST) != 0;
    if (node->is_const) node->is_mutable = 0; // Const implies immutable
    
    node->is_static = (modifiers & MODIFIER_STATIC) != 0;
    
    // By default, pure and clean are TRUE unless proven otherwise
    node->is_pure = !(modifiers & MODIFIER_IMPURE);
    node->has_explicit_pure = (modifiers & MODIFIER_PURE) != 0;
    node->is_pristine = !(modifiers & MODIFIER_TAINTED);
    if (!node->is_pristine) {
        node->var_type.is_tainted = 1;
    } else if (node->var_type.is_tainted) {
        node->is_pristine = 0;
    }
    node->has_explicit_pristine = (modifiers & MODIFIER_PRISTINE) != 0;

    // Variable specific OOP constraints, in case anonymous classes/objects are used
    if (modifiers & MODIFIER_FINAL) {
        node->is_is_a = IS_A_FINAL;
    } else if (modifiers & MODIFIER_NAKED) {
        node->is_is_a = IS_A_NAKED;
    } else {
        node->is_is_a = IS_A_NONE;
    }

    if (modifiers & MODIFIER_INERT) {
        node->is_has_a = HAS_A_INERT;
    } else if (modifiers & MODIFIER_REACTIVE) {
        node->is_has_a = HAS_A_REACTIVE;
    } else {
        node->is_has_a = HAS_A_NONE;
    }
}

void apply_param_modifiers(Parameter* param, int modifiers) {
    param->is_pure = !(modifiers & MODIFIER_IMPURE);
    param->has_explicit_pure = (modifiers & MODIFIER_PURE) != 0;
    param->is_pristine = !(modifiers & MODIFIER_TAINTED);
    if (!param->is_pristine) {
        param->type.is_tainted = 1;
    } else if (param->type.is_tainted) {
        param->is_pristine = 0;
    }
    param->has_explicit_pristine = (modifiers & MODIFIER_PRISTINE) != 0;
}

ASTNode* parse_wash_or_clean_tail(Parser *p, ASTNode *target, WashType wash_type) {
    int line = p->current_token.line, col = p->current_token.col;
    
    char *err_name = NULL;
    ASTNode *body = NULL;
    ASTNode *else_body = NULL;
    
    if (wash_type == WASH_TYPE_UNTAINT) {
        if (p->current_token.type == TOKEN_RESIDUE) {
            eat(p, TOKEN_RESIDUE);
            if (p->current_token.type == TOKEN_LPAREN) {
                eat(p, TOKEN_LPAREN);
                if (p->current_token.type == TOKEN_IDENTIFIER) {
                    err_name = parser_strdup(p, p->current_token.text);
                    eat(p, TOKEN_IDENTIFIER);
                }
                eat(p, TOKEN_RPAREN);
            }
        }
        if (!err_name) {
            err_name = parser_strdup(p, "err");
        }
        body = parse_single_statement_or_block(p);
    } else {
        body = parse_single_statement_or_block(p);
        
        if (p->current_token.type == TOKEN_RESIDUE) {
            eat(p, TOKEN_RESIDUE);
            if (p->current_token.type == TOKEN_LPAREN) {
                eat(p, TOKEN_LPAREN);
                if (p->current_token.type == TOKEN_IDENTIFIER) {
                    err_name = parser_strdup(p, p->current_token.text);
                    eat(p, TOKEN_IDENTIFIER);
                }
                eat(p, TOKEN_RPAREN);
            }
            if (!err_name) {
                err_name = parser_strdup(p, "err");
            }
            
            else_body = parse_single_statement_or_block(p);
        }
    }
    
    WashNode *node = parser_alloc(p, sizeof(WashNode));
    node->base.type = NODE_WASH;
    node->base.line = line;
    node->base.col = col;
    node->target = target;
    node->err_name = err_name;
    node->body = body;
    node->else_body = else_body;
    node->wash_type = wash_type;
    
    return (ASTNode*)node;
}
