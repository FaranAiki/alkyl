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
        } else if (p->current_token.type == TOKEN_ABSTRACT) {
            modifiers |= MODIFIER_ABSTRACT;
            eat(p, TOKEN_ABSTRACT);
        } else if (p->current_token.type == TOKEN_EXACT) {
            modifiers |= MODIFIER_EXACT;
            eat(p, TOKEN_EXACT);
        } else if (p->current_token.type == TOKEN_PRAGMA) {
            modifiers |= MODIFIER_PRAGMA;
            eat(p, TOKEN_PRAGMA);
        } else if (p->current_token.type == TOKEN_METHOD) {
            modifiers |= MODIFIER_METHOD;
            eat(p, TOKEN_METHOD);
        } else if (p->current_token.type == TOKEN_CONTAINER) {
            modifiers |= MODIFIER_CONTAINER;
            eat(p, TOKEN_CONTAINER);
        } else if (p->current_token.type == TOKEN_FRAME) {
            modifiers |= MODIFIER_FRAME;
            eat(p, TOKEN_FRAME);
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
    
    if (modifiers & MODIFIER_ABSTRACT) node->is_abstract = 1;
    if (modifiers & MODIFIER_EXACT) node->is_exact = 1;
    if (modifiers & MODIFIER_PRAGMA) node->is_pragma = 1;
    if (!node->is_abstract && !node->is_exact) node->is_pragma = 1; // default
    
    if (modifiers & MODIFIER_METHOD) node->is_method_class = 1;
    if (modifiers & MODIFIER_CONTAINER) node->is_container = 1;
    if (modifiers & MODIFIER_FRAME) node->is_frame = 1;
    if (!node->is_method_class && !node->is_container) node->is_frame = 1; // default

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

