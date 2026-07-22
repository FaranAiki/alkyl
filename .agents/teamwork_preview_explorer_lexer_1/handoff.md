# Handoff Report: R3: double_quote_as_string setting and string parsing fallback

This report analyzes how to implement the `double_quote_as_string` setting in `LexerSettings` (defaulting to `true`) and the string parsing fallback where `"..."` is treated as a C-string when `false` and as `string(c"...")` when `true`.

---

## 1. Observation

Exact details of structural configurations, parsing locations, and behavior patterns discovered in the codebase:

### 1.1 Lexer Settings Structures & Initialization
- **`include/lexer/lexer.h` (Lines 208–213)**:
  `LexerSettings` currently tracks scopes, comments, spaces, and semicolon requirements:
  ```c
  typedef struct {
      ScopeStyle scope_style;
      CommentStyle comment_style;
      int spaces_per_indent;
      int require_semicolons;  // 1: semicolons are strictly required
  } LexerSettings;
  ```
- **`src/lexer/lexer.c` (Lines 20–29)**:
  Default parameters are configured when no user-provided `LexerSettings` are specified:
  ```c
  if (settings) {
      l->settings = *settings;
  } else {
      // Defaults
      l->settings.scope_style = SCOPE_BRACKETS;
      l->settings.comment_style = COMMENT_SLASH;
      l->settings.spaces_per_indent = 4;
      l->settings.require_semicolons = 1;
  }
  ```

### 1.2 Dynamic Setting Modification via Premeta Blocks
- **`src/parser/top.c` (Lines 275–287)**:
  Dynamic configuration statements (e.g., inside `premeta` blocks) are resolved:
  ```c
  } else if (strcmp(domain, "lexer") == 0) {
      if (strcmp(key, "scope_style") == 0 && val) {
          ...
      } else if (strcmp(key, "require_semicolons") == 0 && val) {
          p->l->settings.require_semicolons = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
      }
  }
  ```

### 1.3 Literal Token Parsing of Double-Quoted Strings
- **`src/parser/expr.c` (Lines 288–298)**:
  `TOKEN_STRING` literals are parsed into heap-managed `string` nodes directly:
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

### 1.4 Non-Expression Usage of `TOKEN_STRING`
- **`src/parser/link.c` (Lines 5–8)**:
  Filepath imports expect `TOKEN_STRING` explicitly:
  ```c
  if (p->current_token.type != TOKEN_STRING) parser_fail(p, "Expected file path string after 'import'");
  char *filename = parser_strdup(p, p->current_token.text);
  eat(p, TOKEN_STRING);
  ```
- **`src/parser/stmt.c` (Lines 120–124)**:
  Parameter start sets use `TOKEN_STRING` but exclude `TOKEN_C_STRING`:
  ```c
  TokenType t = p->current_token.type;
  int is_arg_start = (t == TOKEN_NUMBER || t == TOKEN_SINGLE || t == TOKEN_STRING || 
        t == TOKEN_CHAR_LIT || t == TOKEN_TRUE || t == TOKEN_FALSE || 
        ...
  ```

### 1.5 Object Initialization Lowering
- **`src/semantic/check.c` (Lines 195–211)**:
  Constructor arguments match parameter declarations in the `string` class (`std/string.aky` defines `init(char* raw)`):
  ```c
  if (sym->kind == SYM_CLASS) {
      VarType instance = {TYPE_CLASS, 1, 0, arena_strdup(ctx->compiler_ctx->arena, sym->name), 0, 0, NULL, NULL, 0, 0, 0, 0}; 
      sem_set_node_type(ctx, (ASTNode*)node, instance);
      ...
  }
  ```
- **`src/alir/utils.c` (Lines 56–96)**:
  Constructor calls are lowered by allocating heap space and invoking the initializer:
  ```c
  AlirValue* alir_lower_new_object(AlirCtx *ctx, const char *class_name, ASTNode *args) {
      ...
      AlirInst *call_init = mk_inst(ctx->module, ALIR_OP_CALL, NULL, alir_val_global(ctx->module, class_name, (VarType){TYPE_VOID, 0, 0, NULL}), NULL);
      call_init->args[0] = obj_ptr; // THIS pointer
      ...
      call_init->args[i++] = alir_gen_expr(ctx, a);
      ...
  }
  ```

---

## 2. Logic Chain

1. **Option Propagation**:
   Adding `int double_quote_as_string` to `LexerSettings` in `include/lexer/lexer.h` allows configuration. By setting `l->settings.double_quote_as_string = 1;` in `lexer_init` when defaults are initialized, we ensure the fallback defaults to `true`.
2. **Dynamic Modification Safety**:
   Adding parsing logic for `lexer.double_quote_as_string` in `src/parser/top.c` inside premeta parsing maps values (`"true"`, `"1"` -> `1`; `"false"`, `"0"` -> `0`). Because the parser functions lazy-pull tokens via `lexer_next`, updating the setting inside a `premeta` block dynamically will cleanly apply to subsequent tokens.
3. **Desugaring Location**:
   Changing `"..."` to `TOKEN_C_STRING` at the lexer level when `double_quote_as_string = false` would break other syntax structures. For example, `import "path.aky"` (`src/parser/link.c`) and `reason "reason"` (`src/parser/stmt.c`) check specifically for `TOKEN_STRING`. If double-quotes parsed as `TOKEN_C_STRING`, parsing imports or premeta declarations would fail. Therefore, **we must keep `"..."` tokenized as `TOKEN_STRING` in the lexer** and perform the setting checks and parsing logic inside `src/parser/expr.c` under `parse_primary`.
4. **Target Structure Lowering**:
   - **`double_quote_as_string` is `false`**: `"..."` should be parsed as a C-string (`char*`). This matches `TOKEN_C_STRING`'s AST structure (`TYPE_CHAR` base with `ptr_depth = 1`).
   - **`double_quote_as_string` is `true`**: `"..."` should behave as `string(c"...")`. We can represent this by programmatically generating a `CallNode` targeting a `VarRefNode` named `"string"`, passing a C-string literal (`LiteralNode` of base type `TYPE_CHAR` and pointer depth `1`) containing the token's text. This structure matches standard class instantiations, letting the Semantic Analyzer (`src/semantic/check.c`) and ALIR (`src/alir/utils.c`) translate the node into a standard constructor call to `init(char* raw)`.

---

## 3. Caveats

- **Stdlib Availability**: We assume `class string` (located in `lib/std/string.aky`) is loaded or defined in the namespace when compile-time expression evaluation/lowering occurs, as compiling a double-quoted string expression with `double_quote_as_string` set to `true` depends on the compiler finding the `string` class symbol. If standard files are not imported, semantic validation for the call will fail.
- **Dynamic Scopes**: Since the setting is dynamically checked at expression parse-time, toggling `double_quote_as_string = false` inside `premeta` blocks affects only expressions parsed subsequent to the premeta evaluation.

---

## 4. Conclusion & Recommended Fix Strategy

We recommend implementing this changeset without modifying any scanner/token type assignments in `lexer.c`. The fix strategy should be split into three steps:

### Step 4.1: Extend `LexerSettings`
In `include/lexer/lexer.h`, add `double_quote_as_string` to the `LexerSettings` struct:
```c
typedef struct {
    ScopeStyle scope_style;
    CommentStyle comment_style;
    int spaces_per_indent;
    int require_semicolons;
    int double_quote_as_string; // 1: treat double quotes as string, 0: treat as C-string
} LexerSettings;
```

In `src/lexer/lexer.c` (`lexer_init` function):
```c
  if (settings) {
      l->settings = *settings;
  } else {
      // Defaults
      l->settings.scope_style = SCOPE_BRACKETS;
      l->settings.comment_style = COMMENT_SLASH;
      l->settings.spaces_per_indent = 4;
      l->settings.require_semicolons = 1;
      l->settings.double_quote_as_string = 1; // Default is true
  }
```

### Step 4.2: Support Dynamic Setting inside Premeta Blocks
In `src/parser/top.c`, handle `double_quote_as_string` parsing under the `lexer` domain checks:
```c
                      } else if (strcmp(key, "double_quote_as_string") == 0 && val) {
                          p->l->settings.double_quote_as_string = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
                      }
```

### Step 4.3: Implement AST Desugaring/Parsing Fallback
In `src/parser/expr.c`, rewrite the `TOKEN_STRING` handling case inside `parse_primary`:
```c
  else if (p->current_token.type == TOKEN_STRING) {
    if (p->l->settings.double_quote_as_string) {
      // Treat "..." as string(c"...") CallNode
      VarRefNode *vn = parser_alloc(p, sizeof(VarRefNode));
      vn->base.type = NODE_VAR_REF;
      vn->name = parser_strdup(p, "string");
      set_loc((ASTNode*)vn, line, col);

      // C-string literal argument c"..."
      LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
      ln->base.type = NODE_LITERAL;
      ln->var_type.base = TYPE_CHAR;
      ln->var_type.ptr_depth = 1;
      ln->val.str_val = parser_strdup(p, p->current_token.text);
      set_loc((ASTNode*)ln, line, col);

      // CallNode for constructor
      CallNode *cn = parser_alloc(p, sizeof(CallNode));
      cn->base.type = NODE_CALL;
      cn->name = parser_strdup(p, "string");
      cn->target = (ASTNode*)vn;
      cn->args = (ASTNode*)ln;
      set_loc((ASTNode*)cn, line, col);

      p->current_token.text = NULL; 
      eat(p, TOKEN_STRING);
      node = (ASTNode*)cn;
    } else {
      // Treat "..." as C-string (base = TYPE_CHAR, ptr_depth = 1)
      LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
      ln->base.type = NODE_LITERAL;
      ln->var_type.base = TYPE_CHAR;
      ln->var_type.ptr_depth = 1; 
      ln->val.str_val = parser_strdup(p, p->current_token.text);
      p->current_token.text = NULL; 
      eat(p, TOKEN_STRING);
      node = (ASTNode*)ln;
      set_loc(node, line, col);
    }
  }
```

---

## 5. Verification Method

### 5.1 Verification Commands
Build the compiler binary:
```bash
make
```
Run the test script to confirm no regressions are introduced in existing tests:
```bash
bash scripts/run_tests.sh
```

### 5.2 Test Cases to Validate Feature
Create a test file `test/settings/double_quote.aky`:
```alkyl
import "std/string.aky";
extern int printf(char* fmt, ...);

premeta {
    lexer.double_quote_as_string = false
}

int main() {
    // Under false setting, double quotes produce i8*
    char* c_str = "Hello C-String!";
    printf("%s\n", c_str);

    // Under true setting (default/manually modified), double quotes produce string object on heap
    premeta {
        lexer.double_quote_as_string = true
    }
    string s = "Hello Alkyl String!";
    printf("%s (%d)\n", s.data, s.length());
    return 0;
}
```

Compile and run the test using the built `alkyl` binary:
```bash
./alkyl test/settings/double_quote.aky
./out
```
*Verification Success condition*: Output must correctly print both strings:
```
Hello C-String!
Hello Alkyl String! (19)
```
