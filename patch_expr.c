static int is_unambiguous_expr_start(Parser *p) {
    TokenType t = p->current_token.type;
    return t == TOKEN_IDENTIFIER || t == TOKEN_NUMBER || t == TOKEN_UINT_LIT || t == TOKEN_LONG_LIT ||
           t == TOKEN_ULONG_LIT || t == TOKEN_LONG_LONG_LIT || t == TOKEN_ULONG_LONG_LIT ||
           t == TOKEN_SINGLE_LIT || t == TOKEN_DOUBLE_LIT || t == TOKEN_LONG_DOUBLE_LIT ||
           t == TOKEN_STRING || t == TOKEN_C_STRING || t == TOKEN_BYTE_STRING ||
           t == TOKEN_TRUE || t == TOKEN_FALSE || t == TOKEN_NULL ||
           t == TOKEN_CHAR_LIT || t == TOKEN_LPAREN || t == TOKEN_LBRACKET ||
           t == TOKEN_TYPEOF || t == TOKEN_KW_SIZEOF || t == TOKEN_KW_ALIGNOF ||
           t == TOKEN_KW_DEFINED || t == TOKEN_HASMETHOD || t == TOKEN_HASATTRIBUTE ||
           t == TOKEN_NOT || t == TOKEN_BIT_NOT;
}
