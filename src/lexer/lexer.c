#include "lexer.h"
#include "common.h"
#include "common/diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>

/**
 * @brief Initializes a lexer instance.
 * @param l The lexer to initialize.
 * @param ctx The compiler context.
 * @param filename The name of the source file.
 * @param src The null-terminated source string.
 * @param settings Optional lexer settings; uses defaults if NULL.
 */
void lexer_init(Lexer *l, CompilerContext *ctx, const char *filename, const char* src, LexerSettings *settings) {
  l->src = src;
  l->filename = filename;
  l->pos = 0;
  l->line = 1;
  l->col = 1;
  l->ctx = ctx;
  l->indent_level = 0;
  l->indent_stack[0] = 0;
  l->pending_count = 0;
  l->last_calc_pos = 0;

  if (settings) {
      l->settings = *settings;
  } else {
      // Defaults
      l->settings.scope_style = SCOPE_BRACKETS;
      l->settings.comment_style = COMMENT_SLASH;
      l->settings.spaces_per_indent = 4;
      l->settings.require_semicolons = 1;
      l->settings.double_quote_as_string = 0; // default to 0, user can override in premeta string
      l->settings.import_require_double_quotes = 1;
      l->settings.warning_indent_deep = 4;
  }
}

#define peek(l) ((l)->src[(l)->pos])
/**
 * @brief Advances the lexer by one character and returns it.
 * @param l The lexer instance.
 * @return The character at the current position before advancing.
 */
static inline char advance(Lexer *l) {
    return l->src[l->pos++];
}

/**
 * @brief Interns a string into the compiler's arena allocator.
 * @param l The lexer instance.
 * @param str The null-terminated string to intern.
 * @return A pointer to the interned string.
 */
static char* intern_string(Lexer *l, const char *str) {
    return arena_strdup(l->ctx->arena, str);
}

/**
 * @brief Interns a string of given length into the arena allocator.
 * @param l The lexer instance.
 * @param str The string buffer to intern.
 * @param len The length of the string.
 * @return A pointer to the interned string.
 */
static char* intern_strndup(Lexer *l, const char *str, size_t len) {
    return arena_strndup(l->ctx->arena, str, len);
}

/**
 * @brief Skips whitespace and comments from the current lexer position.
 * @param l The lexer instance.
 */
void skip_whitespace_and_comments(Lexer *l) {
  while (1) {
    char c = peek(l);
    if (c == '\0') break;

    // Hot path for standard spaces/newlines
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
      while (l->src[l->pos] == ' ' || l->src[l->pos] == '\n' || l->src[l->pos] == '\t' || l->src[l->pos] == '\r') {
          l->pos++;
      }
      continue;
    }

    if (isspace((unsigned char)c)) {
      advance(l);
      continue;
    }

    // Single line comment
    if ((l->settings.comment_style == COMMENT_SLASH && c == '/' && l->src[l->pos + 1] == '/') ||
        (l->settings.comment_style == COMMENT_HASH && c == '#')) {
      while (peek(l) != '\0' && peek(l) != '\n') {
        advance(l);
      }
      continue;
    }

    // Block comment
    if (c == '/' && l->src[l->pos + 1] == '*') {
      advance(l); // consume '/'
      advance(l); // consume '*'

      while (1) {
          char next = peek(l);
          if (next == '\0') {
              Token dummy = {TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col};
              report_error(l, dummy, "Unclosed block comment");
              return;
          }

          if (next == '*' && l->src[l->pos + 1] == '/') {
              advance(l); // consume '*'
              advance(l); // consume '/'
              break;
          }

          advance(l);
      }
      continue;
    }

    break;
  }
}

/**
 * @brief Attempts to lex a symbol token at the current position.
 * @param l The lexer instance.
 * @param t The token to fill on success.
 * @return 1 if a symbol was recognized, 0 otherwise.
 */
static int lex_symbol(Lexer *l, Token *t) {
  char c = peek(l);

  if (c == '.') {
      if (l->src[l->pos+1] == '.') {
          if (l->src[l->pos+2] == '.') {
              advance(l); advance(l); advance(l);
              t->type = TOKEN_ELLIPSIS;
              return 1;
          }
          if (l->src[l->pos+2] == '=') {
              advance(l); advance(l); advance(l);
              t->type = TOKEN_RANGE_INCL;
              return 1;
          }
          if (l->src[l->pos+2] == '<') {
              if (l->src[l->pos+3] == '=') {
                  advance(l); advance(l); advance(l); advance(l);
                  t->type = TOKEN_RANGE_INCL_LTE;
                  return 1;
              }
              advance(l); advance(l); advance(l);
              t->type = TOKEN_RANGE_EXCL;
              return 1;
          }
          if (l->src[l->pos+2] == '>') {
              if (l->src[l->pos+3] == '=') {
                  advance(l); advance(l); advance(l); advance(l);
                  t->type = TOKEN_RANGE_INCL_GTE;
                  return 1;
              }
              advance(l); advance(l); advance(l);
              t->type = TOKEN_RANGE_EXCL_GT;
              return 1;
          }
          advance(l); advance(l);
          t->type = TOKEN_RANGE;
          return 1;
      }
      advance(l); t->type = TOKEN_DOT; return 1;
  }

  switch (c) {
      case ',': advance(l); t->type = TOKEN_COMMA; return 1;
      case ':': advance(l); t->type = TOKEN_COLON; return 1;
      case '[': advance(l); t->type = TOKEN_LBRACKET; return 1;
      case ']': advance(l); t->type = TOKEN_RBRACKET; return 1;
      case '{': advance(l); t->type = TOKEN_LBRACE; return 1;
      case '}': advance(l); t->type = TOKEN_RBRACE; return 1;
      case '(': advance(l); t->type = TOKEN_LPAREN; return 1;
      case ')': advance(l); t->type = TOKEN_RPAREN; return 1;
      case ';': advance(l); t->type = TOKEN_SEMICOLON; return 1;
      case '~': advance(l); t->type = TOKEN_BIT_NOT; return 1;
  }

  if (c == '=') {
    advance(l);
    if (peek(l) == '=') { advance(l); t->type = TOKEN_EQ; return 1; }
    t->type = TOKEN_ASSIGN; return 1;
  }

  if (c == '+') {
    advance(l);
    if (peek(l) == '=') { advance(l); t->type = TOKEN_PLUS_ASSIGN; return 1; }
    if (peek(l) == '+') { advance(l); t->type = TOKEN_INCREMENT; return 1; }
    t->type = TOKEN_PLUS; return 1;
  }

  if (c == '-') {
    advance(l);
    if (peek(l) == '=') { advance(l); t->type = TOKEN_MINUS_ASSIGN; return 1; }
    if (peek(l) == '-') { advance(l); t->type = TOKEN_DECREMENT; return 1; }
    t->type = TOKEN_MINUS; return 1;
  }

  if (c == '*') {
    advance(l);
    if (peek(l) == '=') { advance(l); t->type = TOKEN_STAR_ASSIGN; return 1; }
    t->type = TOKEN_STAR; return 1;
  }

  if (c == '/') {
    advance(l);
    if (peek(l) == '=') { advance(l); t->type = TOKEN_SLASH_ASSIGN; return 1; }
    t->type = TOKEN_SLASH; return 1;
  }

  if (c == '%') {
    advance(l);
    if (l->src[l->pos] == '>' && l->src[l->pos + 1] == '>') {
        advance(l); // consume first >
        advance(l); // consume second >
        t->type = TOKEN_RROTATE;
        return 1;
    }
    if (peek(l) == '=') { advance(l); t->type = TOKEN_MOD_ASSIGN; return 1; }
    t->type = TOKEN_MOD; return 1;
  }

  if (c == '&') {
    advance(l);
    if (peek(l) == '&') { advance(l); t->type = TOKEN_AND_AND; return 1; }
    if (peek(l) == '=') { advance(l); t->type = TOKEN_AND_ASSIGN; return 1; }
    t->type = TOKEN_AND; return 1;
  }

  if (c == '|') {
    advance(l);
    if (peek(l) == '|') { advance(l); t->type = TOKEN_OR_OR; return 1; }
    if (peek(l) == '=') { advance(l); t->type = TOKEN_OR_ASSIGN; return 1; }
    t->type = TOKEN_OR; return 1;
  }

  if (c == '^') {
    advance(l);
    if (peek(l) == '=') { advance(l); t->type = TOKEN_XOR_ASSIGN; return 1; }
    t->type = TOKEN_XOR; return 1;
  }

  if (c == '!') {
    advance(l);
    if (peek(l) == '=') { advance(l); t->type = TOKEN_NEQ; return 1; }
    t->type = TOKEN_NOT; return 1;
  }

  if (c == '?') {
    advance(l);
    if (peek(l) == '?') {
      advance(l);
      t->type = TOKEN_QUESTION_QUESTION;
      return 1;
    }
    t->type = TOKEN_QUESTION;
    return 1;
  }
  
  if (c == '$') {
    advance(l);
    t->type = TOKEN_DOLLAR;
    return 1;
  }

  if (c == '@') {
    advance(l);
    t->type = TOKEN_AT;
    return 1;
  }

  if (c == '<') {
    advance(l);
    if (peek(l) == '<') {
        advance(l);
        if (peek(l) == '%') { advance(l); t->type = TOKEN_LROTATE; return 1; }
        if (peek(l) == '=') { advance(l); t->type = TOKEN_LSHIFT_ASSIGN; return 1; }
        t->type = TOKEN_LSHIFT;
        return 1;
    }
    if (peek(l) == '=') { advance(l); t->type = TOKEN_LTE; return 1; }
    t->type = TOKEN_LT; return 1;
  }

  if (c == '>') {
    advance(l);
    if (peek(l) == '>') {
        advance(l);
        if (peek(l) == '=') { advance(l); t->type = TOKEN_RSHIFT_ASSIGN; return 1; }
        t->type = TOKEN_RSHIFT;
        return 1;
    }
    if (peek(l) == '=') { advance(l); t->type = TOKEN_GTE; return 1; }
    t->type = TOKEN_GT; return 1;
  }

  /* lex_symbol_other */

  return 0;
}

/**
 * @brief Attempts to lex a numeric literal at the current position.
 * @param l The lexer instance.
 * @param t The token to fill on success.
 * @return 1 if a number was recognized, 0 otherwise.
 */
static int lex_number(Lexer *l, Token *t) {
    if (!isdigit(peek(l))) return 0;

    int prefix_mode = 0; // 0=none, 1=hex_int, 2=bin_int, 3=hex_single, 4=bin_single, 5=hex_double, 6=bin_double
    char first_digit = peek(l);
    
    char second_char = l->src[l->pos] != '\0' ? l->src[l->pos + 1] : '\0';
    if ((first_digit == '0' || first_digit == '1' || first_digit == '2') && 
        (tolower(second_char) == 'x' || tolower(second_char) == 'b')) {
        if (first_digit == '0' && tolower(second_char) == 'x') prefix_mode = 1;
        else if (first_digit == '0' && tolower(second_char) == 'b') prefix_mode = 2;
        else if (first_digit == '1' && tolower(second_char) == 'x') prefix_mode = 3;
        else if (first_digit == '1' && tolower(second_char) == 'b') prefix_mode = 4;
        else if (first_digit == '2' && tolower(second_char) == 'x') prefix_mode = 5;
        else if (first_digit == '2' && tolower(second_char) == 'b') prefix_mode = 6;
    }

    if (prefix_mode > 0) {
        advance(l); // consume digit
        advance(l); // consume x/b
        unsigned long long val = 0;
        int is_hex = (prefix_mode == 1 || prefix_mode == 3 || prefix_mode == 5);
        
        while (1) {
            char c = peek(l);
            if (is_hex) {
                if (isdigit(c)) val = val * 16 + (c - '0');
                else if (tolower(c) >= 'a' && tolower(c) <= 'f') val = val * 16 + (tolower(c) - 'a' + 10);
                else break;
            } else {
                if (c == '0' || c == '1') val = val * 2 + (c - '0');
                else break;
            }
            advance(l);
        }
        
        if (prefix_mode == 1 || prefix_mode == 2) {
            t->type = TOKEN_NUMBER;
            t->long_val = val;
            
            int is_u = 0, is_l = 0, is_ll = 0;
            while (1) {
                char s = tolower(peek(l));
                if (s == 'u') { is_u = 1; advance(l); }
                else if (s == 'l') {
                    advance(l);
                    if (tolower(peek(l)) == 'l') { is_ll = 1; advance(l); }
                    else { is_l = 1; }
                } else break;
            }
            if (is_ll && is_u) t->type = TOKEN_ULONG_LONG_LIT;
            else if (is_ll) t->type = TOKEN_LONG_LONG_LIT;
            else if (is_l && is_u) t->type = TOKEN_ULONG_LIT;
            else if (is_l) t->type = TOKEN_LONG_LIT;
            else if (is_u) t->type = TOKEN_UINT_LIT;
            
            return 1;
        } else if (prefix_mode == 3 || prefix_mode == 4) {
            t->type = TOKEN_SINGLE_LIT;
            uint32_t raw_val = (uint32_t)val;
            float fval;
            memcpy(&fval, &raw_val, sizeof(float));
            t->double_val = (double)fval;
            return 1;
        } else {
            t->type = TOKEN_DOUBLE_LIT;
            uint64_t raw_val = (uint64_t)val;
            double dval;
            memcpy(&dval, &raw_val, sizeof(double));
            t->double_val = dval;
            return 1;
        }
    }

    const char *start = &l->src[l->pos];
    int length = 0;
    unsigned long long val = 0;

    while (isdigit(peek(l))) {
      val = val * 10 + (peek(l) - '0');
      advance(l);
      length++;
    }

    if (peek(l) == '.' && l->src[l->pos+1] != '.') {
      advance(l);
      length++;

      while (isdigit(peek(l))) {
        advance(l);
        length++;
      }

      char *buf = arena_alloc(l->ctx->arena, length + 1);
      if (!buf) return 0;
      memcpy(buf, start, length);
      buf[length] = '\0';
      double dval = strtod(buf, NULL);

      t->type = TOKEN_DOUBLE_LIT;
      t->double_val = dval;

      // TODO fix this
      if (tolower(peek(l)) == 'l') {
          advance(l);
          t->type = TOKEN_LONG_DOUBLE_LIT;
      } else if (tolower(peek(l)) == 'f') {
          advance(l);
          t->type = TOKEN_SINGLE_LIT;
      }
      return 1;
    }

    int is_u = 0, is_l = 0, is_ll = 0;
    while (1) {
        char s = tolower(peek(l));
        if (s == 'u') { is_u = 1; advance(l); }
        else if (s == 'l') {
            advance(l);
            if (tolower(peek(l)) == 'l') {
                is_ll = 1; advance(l);
            } else {
                is_l = 1;
            }
        } else {
            break;
        }
    }

    t->type = TOKEN_NUMBER;
    t->long_val = val;
    if (is_ll && is_u) t->type = TOKEN_ULONG_LONG_LIT;
    else if (is_ll) t->type = TOKEN_LONG_LONG_LIT;
    else if (is_l && is_u) t->type = TOKEN_ULONG_LIT;
    else if (is_l) t->type = TOKEN_LONG_LIT;
    else if (is_u) t->type = TOKEN_UINT_LIT;

    t->double_val = (double)val;
    t->int_val = (int)val;
    t->long_val = val;
    return 1;
}

/**
 * @brief Attempts to lex a character literal at the current position.
 * @param l The lexer instance.
 * @param t The token to fill on success.
 * @return 1 if a character literal was recognized, 0 otherwise.
 */
static int lex_char(Lexer *l, Token *t) {
    if (peek(l) != '\'') return 0;

    advance(l);
    char val = advance(l);

    if (val == '\\') {
        char next = advance(l);
        switch(next) {
            case 'n': val = '\n'; break;
            case 't': val = '\t'; break;
            case 'r': val = '\r'; break;
            case '0': val = '\0'; break;
            case '\'': val = '\''; break;
            case '\\': val = '\\'; break;
            default: val = next; break;
        }
    }

    if (peek(l) == '\'') {
        advance(l);
    } else {
        Token dummy = {TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col};
        report_error(l, dummy, "Unclosed character literal");
    }

    t->type = TOKEN_CHAR_LIT;
    t->int_val = (int)val;
    t->long_val = (unsigned long long)val;
    return 1;
}

// Dynamically scales using StringBuilder, eliminating hard limits and data loss.
/**
 * @brief Consumes and returns the content of a string literal, handling escapes.
 * @param l The lexer instance.
 * @return The interned string content.
 */
static char* consume_string_content(Lexer *l) {
    StringBuilder sb;
    sb_init(&sb, l->ctx->arena);

    while (peek(l) != '"' && peek(l) != '\0') {
      char val = peek(l);
      if (val == '\\') {
        advance(l);
        if (peek(l) == '\0') break;
        char escaped = peek(l);
        switch (escaped) {
          case 'n': val = '\n'; break;
          case 'r': val = '\r'; break;
          case 't': val = '\t'; break;
          case '0': val = '\0'; break;
          case '\\': val = '\\'; break;
          case '"': val = '"'; break;
          case '\'': val = '\''; break;
          default: val = escaped; break;
        }
        advance(l);
      } else {
        advance(l);
      }
      sb_append_c(&sb, val);
    }

    if (peek(l) == '"') advance(l);

    // Check pool/allocate into pool safely
    char *final_str = intern_string(l, sb.data ? sb.data : "");
    sb_free(&sb);
    return final_str;
}

/**
 * @brief Attempts to lex a string literal at the current position.
 * @param l The lexer instance.
 * @param t The token to fill on success.
 * @return 1 if a string was recognized, 0 otherwise.
 */
static int lex_string(Lexer *l, Token *t) {
  char c = peek(l);

  // C-String check: c"..."
  if (c == 'c' && l->src[l->pos + 1] == '"') {
    advance(l); // consume 'c'
    advance(l); // consume '"'

    t->type = TOKEN_C_STRING;
    t->text = consume_string_content(l);
    return 1;
  }

  // Byte string check: b"..."
  if (c == 'b' && l->src[l->pos + 1] == '"') {
    advance(l); // consume 'b'
    advance(l); // consume '"'

    t->type = TOKEN_BYTE_STRING;
    t->text = consume_string_content(l);
    return 1;
  }

  if (c == '"') {
    advance(l);
    t->type = l->settings.double_quote_as_string ? TOKEN_STRING : TOKEN_C_STRING;
    t->text = consume_string_content(l);
    return 1;
  }

  return 0;
}

// O(1) Hash Table for Keywords
#define KW_HASH_SIZE 256
static KeywordDef kw_hash[KW_HASH_SIZE];
static int kw_hash_init = 0;

/**
 * @brief Computes a hash for a null-terminated string.
 * @param str The input string.
 * @return The computed hash value.
 */
static unsigned int hash_str(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

/**
 * @brief Initializes the keyword hash table on first use.
 */
static void init_kw_hash() {
    if (kw_hash_init) return;
    int num_keywords = sizeof(keywords) / sizeof(keywords[0]);
    for (int i = 0; i < num_keywords; i++) {
        unsigned int h = hash_str(keywords[i].word) % KW_HASH_SIZE;
        while (kw_hash[h].word != NULL) {
            h = (h + 1) % KW_HASH_SIZE;
        }
        kw_hash[h] = keywords[i];
    }
    kw_hash_init = 1;
}

/**
 * @brief Computes a hash for a string of given length.
 * @param str The input string buffer.
 * @param len The length of the string.
 * @return The computed hash value.
 */
static unsigned int hash_strn(const char *str, size_t len) {
    unsigned int hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + str[i];
    }
    return hash;
}

/**
 * @brief Checks if a character is valid at the start of an identifier.
 * @param c The character to check.
 * @return Non-zero if the character can start an identifier.
 */
static int is_ident_start(char c) {
    unsigned char uc = (unsigned char)c;
    return isalpha(uc) || uc == '_' || uc >= 0x80;
}

/**
 * @brief Checks if a character is valid inside an identifier.
 * @param c The character to check.
 * @return Non-zero if the character can appear inside an identifier.
 */
static int is_ident_part(char c) {
    unsigned char uc = (unsigned char)c;
    return isalnum(uc) || uc == '_' || uc >= 0x80;
}

/**
 * @brief Attempts to lex an identifier or keyword at the current position.
 * @param l The lexer instance.
 * @param t The token to fill on success.
 * @return 1 if an identifier/keyword was recognized, 0 otherwise.
 */
static int lex_word(Lexer *l, Token *t) {
  char c = peek(l);
  if (!is_ident_start(c)) return 0;

  const char *start = l->src + l->pos;
  int length = 0;
  while (is_ident_part(peek(l))) {
      advance(l);
      length++;
  }

  init_kw_hash();
  unsigned int h = hash_strn(start, length) % KW_HASH_SIZE;
  while (kw_hash[h].word != NULL) {
      if (strncmp(kw_hash[h].word, start, length) == 0 && kw_hash[h].word[length] == '\0') {
          t->type = kw_hash[h].type;
          return 1;
      }
      h = (h + 1) % KW_HASH_SIZE;
  }

  // Fallback to identifier
  t->type = TOKEN_IDENTIFIER;
  t->text = intern_strndup(l, start, length);
  return 1;
}


/**
 * @brief Advances the lexer and returns the next token.
 * @param l The lexer instance.
 * @return The next token in the source stream.
 */
Token lexer_next(Lexer *l) {
  if (l->pending_count > 0) {
      Token t = l->pending_tokens[0];
      for (int i = 1; i < l->pending_count; i++) {
          l->pending_tokens[i-1] = l->pending_tokens[i];
      }
      l->pending_count--;
      return t;
  }

  int prev_line = l->line;
  int is_first_token = (l->pos == 0);

  int start_pos_before_skip = l->pos;
  skip_whitespace_and_comments(l);
  int has_space_before = (l->pos > start_pos_before_skip) || is_first_token;

  // Catch up line and col to l->pos
  for (int i = l->last_calc_pos; i < l->pos; i++) {
      if (l->src[i] == '\n') {
          l->line++;
          l->col = 1;
      } else {
          l->col++;
      }
  }
  l->last_calc_pos = l->pos;

  int is_eof = (peek(l) == '\0');
  int is_new_line = (l->line > prev_line) || is_first_token || is_eof;

  if (l->settings.scope_style == SCOPE_INDENTATION && is_new_line) {
      int spaces = l->col - 1;
      int new_indent = l->settings.spaces_per_indent > 0 ? (spaces / l->settings.spaces_per_indent) : 0;

      if (is_eof) {
          new_indent = 0; // EOF forces indent to 0
      }

      if (new_indent > l->indent_level) {
          if (l->pending_count >= 16) {
              report_error(l, (Token){TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col}, "Indentation too deep");
              return (Token){TOKEN_EOF, NULL, 0, 0, 0.0, l->line, l->col};
          }
          if (l->indent_level >= 127) {
              report_error(l, (Token){TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col}, "Indentation too deep");
              return (Token){TOKEN_EOF, NULL, 0, 0, 0.0, l->line, l->col};
          }
           Token lbrace = {TOKEN_LBRACE, NULL, 0, 0, 0.0, l->line, 1};
           l->pending_tokens[l->pending_count++] = lbrace;
           l->indent_stack[++l->indent_level] = new_indent;
           if (l->settings.warning_indent_deep > 0 && l->indent_level >= l->settings.warning_indent_deep) {
               char warn_msg[128];
               snprintf(warn_msg, sizeof(warn_msg), "Indentation is more than %d levels deep; consider refactoring", l->settings.warning_indent_deep - 1);
               report_warning(l, (Token){TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col}, warn_msg);
           }
      } else if (new_indent < l->indent_level) {
          while (l->indent_level > 0 && l->indent_stack[l->indent_level] > new_indent) {
              if (l->pending_count >= 16) {
                  report_error(l, (Token){TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col}, "Indentation too deep");
                  return (Token){TOKEN_EOF, NULL, 0, 0, 0.0, l->line, l->col};
              }
              Token rbrace = {TOKEN_RBRACE, NULL, 0, 0, 0.0, l->line, 1};
              l->pending_tokens[l->pending_count++] = rbrace;
              l->indent_level--;
          }
      }
  }

  Token t = {TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col, 0, has_space_before};
  int start_pos = l->pos;
  char c = peek(l);

  int token_parsed = 0;
  if (c == '\0') {
    t.type = TOKEN_EOF;
    token_parsed = 1;
  }

  if (!token_parsed && lex_symbol(l, &t)) token_parsed = 1;
  if (!token_parsed && lex_number(l, &t)) token_parsed = 1;
  if (!token_parsed && lex_char(l, &t)) token_parsed = 1;
  if (!token_parsed && lex_string(l, &t)) token_parsed = 1;
  if (!token_parsed && lex_word(l, &t)) token_parsed = 1;

  if (!token_parsed) {
    advance(l);
    t.type = TOKEN_UNKNOWN;
  }

  // Catch up line and col to the end of the token
  for (int i = l->last_calc_pos; i < l->pos; i++) {
      if (l->src[i] == '\n') {
          l->line++;
          l->col = 1;
      } else {
          l->col++;
      }
  }
  l->last_calc_pos = l->pos;

  t.length = l->pos - start_pos;

  if (l->pending_count > 0) {
      if (t.type != TOKEN_EOF) {
          if (l->pending_count >= 16) {
              report_error(l, (Token){TOKEN_UNKNOWN, NULL, 0, 0, 0.0, l->line, l->col}, "Too many pending tokens");
              return t;
          }
          l->pending_tokens[l->pending_count++] = t;
      }
      Token first = l->pending_tokens[0];
      for (int i = 1; i < l->pending_count; i++) {
          l->pending_tokens[i-1] = l->pending_tokens[i];
      }
      l->pending_count--;
      return first;
  }

  return t;
}
