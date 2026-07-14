# Handoff Report: String Parsing Fallback and LexerSettings Analysis

## 1. Observation
The following locations in the codebase define and handle string tokenization and AST parsing:

- **`include/lexer/lexer.h:208-213`**: Definition of `LexerSettings`:
  ```c
  typedef struct {
      ScopeStyle scope_style;
      CommentStyle comment_style;
      int spaces_per_indent;
      int require_semicolons;  // 1: semicolons are strictly required
  } LexerSettings;
  ```

- **`src/lexer/lexer.c:9-29`**: Initialization of `LexerSettings` defaults when `settings` is `NULL`:
  ```c
  void lexer_init(Lexer *l, CompilerContext *ctx, const char *filename, const char* src, LexerSettings *settings) {
    ...
    if (settings) {
        l->settings = *settings;
    } else {
        // Defaults
        l->settings.scope_style = SCOPE_BRACKETS;
        l->settings.comment_style = COMMENT_SLASH;
        l->settings.spaces_per_indent = 4;
        l->settings.require_semicolons = 1;
    }
  }
  ```

- **`src/parser/expr.c:288-298`**: Parsing of double-quoted `TOKEN_STRING` literals into AST:
  ```c
    else if (p->current_token.type == TOKEN_STRING) {
      LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
      ln->base.type = NODE_LITERAL;
      ln->var_type.base = TYPE_CLASS;
      ln->var_type.class_name = parser_strdup(p, "string");
      ln->val.str_val = parser_strdup(p, p->current_token.text);
      p->current_token.text = NULL; 
      eat(p, TOKEN_STRING);
      node = (ASTNode*)ln;
      set_loc(node, line, col);
    }
  ```

- **`src/parser/expr.c:299-309`**: Parsing of C-style `TOKEN_C_STRING` literals:
  ```c
    else if (p->current_token.type == TOKEN_C_STRING) {
      LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
      ln->base.type = NODE_LITERAL;
      ln->var_type.base = TYPE_CHAR;
      ln->var_type.ptr_depth = 1; 
      ln->val.str_val = parser_strdup(p, p->current_token.text);
      p->current_token.text = NULL; 
      eat(p, TOKEN_C_STRING);
      node = (ASTNode*)ln;
      set_loc(node, line, col);
    }
  ```

- **Metadata parsing expecting `TOKEN_STRING`**:
  - `src/parser/link.c:5`: `if (p->current_token.type != TOKEN_STRING) parser_fail(p, "Expected file path string after 'import'");`
  - `src/parser/stmt.c:211`: `if (p->current_token.type != TOKEN_STRING) parser_fail(p, "Expected string literal after reason");`
  - `src/parser/top.c:172`: `if (p->current_token.type != TOKEN_STRING) parser_fail(p, "Expected string literal after reason");`

---

## 2. Logic Chain
To implement the `double_quote_as_string` setting and fallback logic:

1. **Setting Addition**: We need to add `int double_quote_as_string` to `LexerSettings` in `include/lexer/lexer.h`. In `src/lexer/lexer.c:lexer_init`, it must default to `1` (true).
2. **Translation Layer (Lexer vs. Parser)**:
   - If the lexer translates double-quoted strings directly to `TOKEN_C_STRING` when `double_quote_as_string == false`, parser directives like `import "path.aky"` or `reason "msg"` will fail. These expect `TOKEN_STRING` explicitly.
   - Therefore, the lexer must continue tokenizing `"..."` as `TOKEN_STRING`.
   - The fallback translation should happen inside `src/parser/expr.c:parse_primary` when parsing expression literals.
3. **AST Construction**:
   - **Case `double_quote_as_string == false`**: We treat `"..."` as `c"..."`. We construct a `LiteralNode` of type `char*` (`TYPE_CHAR` with `ptr_depth = 1`).
   - **Case `double_quote_as_string == true`**: We treat `"..."` as `string(c"...")`.
     - We construct a `LiteralNode` for the argument `c"..."` (type `char*`).
     - We construct a `VarRefNode` for the class identifier `"string"`.
     - We construct a `CallNode` with the target pointing to the `VarRefNode` and arguments pointing to the `LiteralNode`.
     - This node is then fed into semantic checking. It compiles using standard class instantiation code paths (`sem_check_call` -> `alir_lower_new_object` -> `init(char*)` constructor call).

---

## 3. Caveats
- **Performance & Heap Overhead**: Transforming `"..."` to `string(c"...")` at the AST level compiles the string instantiation using `alir_lower_new_object` (which issues heap allocation via `malloc` and dynamic `init(char*)` initialization). In contrast, the current compiler backend statically instantiates `TOKEN_STRING` as a global constant structure. Thus, this fallback strategy will introduce runtime heap overhead for every evaluated double-quoted string literal.
- **Dependency on `std/string`**: If `double_quote_as_string == true`, double-quoted strings will fail compilation with `"Undefined function or class 'string'"` if the standard library `std/string` class is not imported.

---

## 4. Conclusion
We recommend the following fix strategy:

1. **Modify `include/lexer/lexer.h`**:
   Add `int double_quote_as_string;` to `LexerSettings`.
2. **Modify `src/lexer/lexer.c`**:
   In `lexer_init`, set `l->settings.double_quote_as_string = 1;` when settings parameter is NULL.
3. **Modify `src/parser/expr.c`**:
   Replace the `TOKEN_STRING` branch in `parse_primary` with:
   ```c
   else if (p->current_token.type == TOKEN_STRING) {
     if (!p->l->settings.double_quote_as_string) {
       // Treat as C-string: c"..."
       LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
       ln->base.type = NODE_LITERAL;
       ln->var_type.base = TYPE_CHAR;
       ln->var_type.ptr_depth = 1; 
       ln->val.str_val = parser_strdup(p, p->current_token.text);
       p->current_token.text = NULL; 
       eat(p, TOKEN_STRING);
       node = (ASTNode*)ln;
       set_loc(node, line, col);
     } else {
       // Treat as string(c"...") constructor call
       LiteralNode *c_str_node = parser_alloc(p, sizeof(LiteralNode));
       c_str_node->base.type = NODE_LITERAL;
       c_str_node->var_type.base = TYPE_CHAR;
       c_str_node->var_type.ptr_depth = 1;
       c_str_node->val.str_val = parser_strdup(p, p->current_token.text);
       c_str_node->base.next = NULL;
       set_loc((ASTNode*)c_str_node, line, col);

       VarRefNode *var_ref = parser_alloc(p, sizeof(VarRefNode));
       var_ref->base.type = NODE_VAR_REF;
       var_ref->name = parser_strdup(p, "string");
       var_ref->mangled_name = NULL;
       var_ref->is_class_member = false;
       set_loc((ASTNode*)var_ref, line, col);

       CallNode *call = parser_alloc(p, sizeof(CallNode));
       call->base.type = NODE_CALL;
       call->name = parser_strdup(p, "string");
       call->target = (ASTNode*)var_ref;
       call->args = (ASTNode*)c_str_node;
       set_loc((ASTNode*)call, line, col);

       p->current_token.text = NULL;
       eat(p, TOKEN_STRING);
       node = (ASTNode*)call;
     }
   }
   ```

---

## 5. Verification Method
- **Verification via AST dump**:
  1. Build the compiler: `make`
  2. Compile a test case using double quotes (e.g. `test/string/test_string.aky`).
  3. Inspect `out.ast`. Under the default setting, the string literal `"hello world"` must be parsed into a `NODE_CALL` targetting `string` with a `NODE_LITERAL` C-string argument.
  4. Write a custom compiler test where `LexerSettings.double_quote_as_string = 0`. Verify that `out.ast` prints a `NODE_LITERAL` of type `char*` directly, and that `import` statements still parse correctly.
