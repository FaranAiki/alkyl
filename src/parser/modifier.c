#include "parser_internal.h"
#include <string.h>

// Parses sequential modifier keywords and encodes them into a single bitmask
int parse_modifiers(Parser* p) {
    int modifiers = 0;
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
    
    // Inherited rules for functions (e.g. final overriding rules)
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
