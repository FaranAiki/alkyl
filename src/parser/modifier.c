/**
 * @file modifier.c
 * @brief Modifier parsing and application for the Alkyl parser.
 */
#include "parser.h"
#include "modifier.h"
#include <string.h>

/**
 * @brief Parses a sequence of modifier tokens from the current position.
 * @param p Parser context.
 * @return Bitmask of parsed modifier flags.
 */
int parse_modifiers(Parser* p) {
    int modifiers = 0;

    while (1) {
        switch (p->current_token.type) {
            case TOKEN_PUBLIC: modifiers |= MODIFIER_PUBLIC; eat(p, TOKEN_PUBLIC); break;
            case TOKEN_PRIVATE: modifiers |= MODIFIER_PRIVATE; eat(p, TOKEN_PRIVATE); break;
            case TOKEN_OPEN: modifiers |= MODIFIER_OPEN; eat(p, TOKEN_OPEN); break;
            case TOKEN_CLOSED: modifiers |= MODIFIER_CLOSED; eat(p, TOKEN_CLOSED); break;
            case TOKEN_CONST: modifiers |= MODIFIER_CONST; eat(p, TOKEN_CONST); break;
            case TOKEN_INERT: modifiers |= MODIFIER_INERT; eat(p, TOKEN_INERT); break;
            case TOKEN_REACTIVE: modifiers |= MODIFIER_REACTIVE; eat(p, TOKEN_REACTIVE); break;
            case TOKEN_NAKED: modifiers |= MODIFIER_NAKED; eat(p, TOKEN_NAKED); break;
            case TOKEN_PURE: modifiers |= MODIFIER_PURE; eat(p, TOKEN_PURE); break;
            case TOKEN_IMPURE: modifiers |= MODIFIER_IMPURE; eat(p, TOKEN_IMPURE); break;
            case TOKEN_PRISTINE: modifiers |= MODIFIER_PRISTINE; eat(p, TOKEN_PRISTINE); break;
            case TOKEN_TAINTED: modifiers |= MODIFIER_TAINTED; eat(p, TOKEN_TAINTED); break;
            case TOKEN_COVALENT: modifiers |= MODIFIER_COVALENT; eat(p, TOKEN_COVALENT); break;
            case TOKEN_ABSTRACT: modifiers |= MODIFIER_ABSTRACT; eat(p, TOKEN_ABSTRACT); break;
            case TOKEN_PARTIAL: modifiers |= MODIFIER_PARTIAL; eat(p, TOKEN_PARTIAL); break;
            case TOKEN_TOTAL: modifiers |= MODIFIER_TOTAL; eat(p, TOKEN_TOTAL); break;
            case TOKEN_EXACT: modifiers |= MODIFIER_EXACT; eat(p, TOKEN_EXACT); break;
            case TOKEN_PRAGMA: modifiers |= MODIFIER_PRAGMA; eat(p, TOKEN_PRAGMA); break;
            case TOKEN_METHOD: modifiers |= MODIFIER_METHOD; eat(p, TOKEN_METHOD); break;
            case TOKEN_KW_MUT: modifiers |= MODIFIER_MUTABLE; eat(p, TOKEN_KW_MUT); break;
            case TOKEN_EXTENDED: modifiers |= MODIFIER_EXTENDED; eat(p, TOKEN_EXTENDED); break;
            case TOKEN_OVERRIDE: modifiers |= MODIFIER_OVERRIDE; eat(p, TOKEN_OVERRIDE); break;
            case TOKEN_CONTAINER: modifiers |= MODIFIER_CONTAINER; eat(p, TOKEN_CONTAINER); break;
            case TOKEN_EXPORT: modifiers |= MODIFIER_EXPORT; eat(p, TOKEN_EXPORT); break;
            case TOKEN_FRAME: modifiers |= MODIFIER_FRAME; eat(p, TOKEN_FRAME); break;
            case TOKEN_META: {
                TokenType next = parser_peek_token(p).type;
                if (next == TOKEN_LBRACE || next == TOKEN_LBRACKET || next == TOKEN_LPAREN || next == TOKEN_IF || next == TOKEN_WHILE) {
                    return modifiers; // Not a modifier! Let top.c or stmt.c handle it.
                }
                modifiers |= MODIFIER_META;
                eat(p, TOKEN_META);
                break;
            }
            case TOKEN_AT: {
                eat(p, TOKEN_AT);
                if (p->current_token.type != TOKEN_IDENTIFIER) {
                    parser_fail(p, "Expected identifier after '@'");
                    return modifiers;
                }
                p->pending_cconv = parser_strdup(p, p->current_token.text);
                eat(p, TOKEN_IDENTIFIER);
                break;
            }
            case TOKEN_IDENTIFIER: {
                if (streq_lit(p->current_token.text, "static")) {
                    modifiers |= MODIFIER_STATIC;
                    eat(p, TOKEN_IDENTIFIER);
                    break;
                }
                return modifiers;
            }
            default:
                return modifiers;
        }
    }
}

// Applies extracted modifiers correctly to ClassNodes
/**
 * @brief Applies a modifier bitmask to a class node.
 * @param node Class node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
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
    if (modifiers & MODIFIER_EXTENDED) node->is_extended = 1;
    if (modifiers & MODIFIER_FRAME) node->is_frame = 1;
    if (!node->is_method_class && !node->is_container) node->is_frame = 1; // default

    node->is_pure = !(modifiers & MODIFIER_IMPURE);
    node->has_explicit_pure = (modifiers & MODIFIER_PURE) != 0;
    
    // IS-A constraints (Inheritance)
    if (modifiers & MODIFIER_CLOSED) {
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
/**
 * @brief Applies a modifier bitmask to a function definition node.
 * @param node Function definition node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
void apply_func_modifiers(FuncDefNode* node, int modifiers) {
    if (modifiers & MODIFIER_PUBLIC) node->is_public = 1;
    if (modifiers & MODIFIER_PRIVATE) node->is_public = 0;
    if (modifiers & MODIFIER_OPEN) node->is_open = 1;
    if (modifiers & MODIFIER_CLOSED) node->is_open = 0;
    if (modifiers & MODIFIER_STATIC) node->is_static = 1;
    if (modifiers & MODIFIER_OVERRIDE) node->is_override = 1;
    if (modifiers & MODIFIER_MUTABLE) node->is_mutable = 1;
    if (modifiers & MODIFIER_META) node->is_macro = 1;
    if (modifiers & MODIFIER_COVALENT) node->is_covalent = 1;
    
    // node is pure is TRUE by default unless proven otherwise (e.g. explicitly impure)
    if (node->is_extern) {
        node->is_pure = (modifiers & MODIFIER_PURE) != 0;
    } else {
        node->is_pure = !(modifiers & MODIFIER_IMPURE);
    }
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

    node->is_total = !(modifiers & MODIFIER_PARTIAL);
    node->has_explicit_total = (modifiers & MODIFIER_TOTAL) != 0;
    node->is_partial = (modifiers & MODIFIER_PARTIAL) != 0;
    node->has_explicit_partial = (modifiers & MODIFIER_PARTIAL) != 0;

}

// Applies extracted modifiers correctly to VarDeclNodes
/**
 * @brief Applies a modifier bitmask to a variable declaration node.
 * @param node Variable declaration node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
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


}

/**
 * @brief Applies a modifier bitmask to a parameter node.
 * @param param Parameter node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
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


/**
 * @brief Dispatches modifier application to the appropriate typed function.
 * @param node Generic AST node to modify.
 * @param modifiers Bitmask of modifier flags.
 */
void apply_modifiers_to_node(ASTNode *node, int modifiers) {
    if (!node) return;
    if (node->type == NODE_CLASS) apply_class_modifiers((ClassNode*)node, modifiers);
    else if (node->type == NODE_FUNC_DEF) apply_func_modifiers((FuncDefNode*)node, modifiers);
    else if (node->type == NODE_VAR_DECL) apply_var_modifiers((VarDeclNode*)node, modifiers);
}
