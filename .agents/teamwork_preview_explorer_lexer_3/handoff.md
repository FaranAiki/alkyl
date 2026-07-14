# R3: double_quote_as_string Settings and Fallback Investigation Report

## 1. Observation
The following file locations and behaviors were observed in the Alkyl codebase:

1. **LexerSettings Struct (`include/lexer/lexer.h:208-213`)**:
   ```c
   typedef struct {
       ScopeStyle scope_style;
       CommentStyle comment_style;
       int spaces_per_indent;
       int require_semicolons;  // 1: semicolons are strictly required
   } LexerSettings;
   ```

2. **Lexer Initialization (`src/lexer/lexer.c:9-29`)**:
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
   All calls to `lexer_init` in the compiler pass `NULL` as the `settings` argument, relying entirely on default initialization.

3. **Double Quote Lexing (`src/lexer/lexer.c:414-419`)**:
   ```c
     if (c == '"') {
     advance(l); 
     t->type = TOKEN_STRING;
     t->text = consume_string_content(l);
     return 1;
   }
   ```
   Double quotes `"..."` currently produce a `TOKEN_STRING` token type.

4. **Literal Node Parser Handling (`src/parser/expr.c:288-298`)**:
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

5. **Non-Expression String Tokens usage**:
   The parser checks for and eats `TOKEN_STRING` directly for non-expression syntax elements such as:
   - Imports/links in `src/parser/link.c` (e.g., `import "lib/c"`)
   - Pre-meta block keys/values in `src/parser/top.c`
   - Attributes/reasons in `src/parser/stmt.c` and `src/parser/top.c`

6. **Premeta Block Config Parsing (`src/parser/top.c:275-287`)**:
   ```c
                     } else if (strcmp(domain, "lexer") == 0) {
                         if (strcmp(key, "scope_style") == 0 && val) {
                             ...
                         } else if (strcmp(key, "require_semicolons") == 0 && val) {
                             p->l->settings.require_semicolons = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
                         }
                     }
   ```

## 2. Logic Chain
1. If we change the lexer to produce `TOKEN_C_STRING` directly when `double_quote_as_string` is false, it will break parsing of non-expression parts like `import "lib/c"`, custom reasons, annotations, and attributes (Observation 5) because these specifically expect `TOKEN_STRING`.
2. Therefore, the lexer must continue to yield `TOKEN_STRING` for double-quoted strings `"..."` regardless of settings.
3. Since the setting `double_quote_as_string` resides in `LexerSettings` which is accessible via `p->l->settings`, the expression parser in `parse_factor` (Observation 4) has full access to query the setting.
4. If `double_quote_as_string` is false, `"..."` must be treated as a C-string literal. This matches the existing handling of `TOKEN_C_STRING`, which constructs a `LiteralNode` of base type `TYPE_CHAR` and `ptr_depth` = 1.
5. If `double_quote_as_string` is true, `"..."` must be treated as `string(c"...")`. This matches a call to the `string` class constructor with the C-string as its single argument. We can construct this AST representation dynamically in `parse_factor` as a `CallNode` wrapping a `VarRefNode` for `"string"` and passing a `LiteralNode` argument of type `char*` with the string text.
6. The constructor call representation is compatible with down-stream semantic checks (`sem_check_call` and `sem_check_call_args`) and ALIR code generation (`alir_lower_new_object`), ensuring type-correct instantiations of the `string` class without modifying semantic analysis or IR-lowering layers.

## 3. Caveats
- This investigation assumes that the standard library class `string` with its constructor `init(char*)` is linked / imported correctly by the program using `"lib/std/string.aky"` or similar standard library includes.
- Compile-time VM interpreter `print` statement execution (`src/meta/vm.c`) prints temporary values (`ALIR_VAL_TEMP`) as integer addresses. Therefore, print statements targeting `string` class values will print the pointer value unless standard output printing for classes is implemented. This is consistent with current VM limitations.

## 4. Conclusion
We recommend implementing this change entirely within the header `include/lexer/lexer.h` and the lexer/parser sources (`src/lexer/lexer.c`, `src/parser/top.c`, `src/parser/expr.c`), keeping the lexer's token output simple and desugaring the string literals at parser expression level.

### Fix Strategy Diffs

#### 1. Add Setting to Struct in `include/lexer/lexer.h`
```diff
--- include/lexer/lexer.h
+++ include/lexer/lexer.h
@@ -212,3 +212,4 @@
     int require_semicolons;  // 1: semicolons are strictly required
+    int double_quote_as_string; // 1: treat "..." as string(c"..."), 0: treat "..." as c"..."
 } LexerSettings;
```

#### 2. Default Initializer in `src/lexer/lexer.c`
```diff
--- src/lexer/lexer.c
+++ src/lexer/lexer.c
@@ -27,2 +27,3 @@
       l->settings.require_semicolons = 1;
+      l->settings.double_quote_as_string = 1; // Default is true
   }
```

#### 3. Parse Premeta Settings in `src/parser/top.c`
```diff
--- src/parser/top.c
+++ src/parser/top.c
@@ -286,2 +286,4 @@
                           p->l->settings.require_semicolons = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
+                      } else if (strcmp(key, "double_quote_as_string") == 0 && val) {
+                          p->l->settings.double_quote_as_string = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
                       }
```

#### 4. Transform AST representation in `src/parser/expr.c`
```diff
--- src/parser/expr.c
+++ src/parser/expr.c
@@ -288,11 +288,36 @@
   else if (p->current_token.type == TOKEN_STRING) {
-    LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
-    ln->base.type = NODE_LITERAL;
-    ln->var_type.base = TYPE_CLASS;
-    ln->var_type.class_name = parser_strdup(p, "string");
-    ln->val.str_val = parser_strdup(p, p->current_token.text);
-    p->current_token.text = NULL; 
-    eat(p, TOKEN_STRING);
-    node = (ASTNode*)ln;
-    set_loc(node, line, col);
+    if (p->l->settings.double_quote_as_string) {
+      // Treat as string(c"...")
+      
+      // 1. Create argument C-string literal node: c"..."
+      LiteralNode *arg_ln = parser_alloc(p, sizeof(LiteralNode));
+      arg_ln->base.type = NODE_LITERAL;
+      arg_ln->var_type.base = TYPE_CHAR;
+      arg_ln->var_type.ptr_depth = 1;
+      arg_ln->val.str_val = parser_strdup(p, p->current_token.text);
+      arg_ln->base.next = NULL;
+      set_loc((ASTNode*)arg_ln, line, col);
+      
+      // 2. Create target class/function variable reference: string
+      VarRefNode *target_vn = parser_alloc(p, sizeof(VarRefNode));
+      target_vn->base.type = NODE_VAR_REF;
+      target_vn->name = parser_strdup(p, "string");
+      set_loc((ASTNode*)target_vn, line, col);
+      
+      // 3. Create CallNode: string(c"...")
+      CallNode *call_node = parser_alloc(p, sizeof(CallNode));
+      call_node->base.type = NODE_CALL;
+      call_node->name = parser_strdup(p, "string");
+      call_node->target = (ASTNode*)target_vn;
+      call_node->args = (ASTNode*)arg_ln;
+      set_loc((ASTNode*)call_node, line, col);
+      
+      p->current_token.text = NULL;
+      eat(p, TOKEN_STRING);
+      node = (ASTNode*)call_node;
+    } else {
+      // Treat as C-string: c"..."
+      LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
+      ln->base.type = NODE_LITERAL;
+      ln->var_type.base = TYPE_CHAR;
+      ln->var_type.ptr_depth = 1;
+      ln->val.str_val = parser_strdup(p, p->current_token.text);
+      p->current_token.text = NULL;
+      eat(p, TOKEN_STRING);
+      node = (ASTNode*)ln;
+      set_loc(node, line, col);
+    }
   }
```

## 5. Verification Method
1. **Rebuild the compiler**:
   ```bash
   cmake -B build
   cmake --build build
   ```
2. **Execute tests**:
   Ensure all existing tests pass by running:
   ```bash
   ./scripts/run_tests.sh
   ```
3. **Validate setting behavior via test file**:
   Create a test script `test_lexer.aky` containing:
   ```hky
   extern int printf(char* fmt, ...);
   extern void* malloc(unsigned int size);
   import "lib/std/string.aky"

   premeta {
       lexer.double_quote_as_string = true
       reason "Default behavior validation"
   }

   int main() {
       // With double_quote_as_string = true (default), 
       // "hello" behaves as a string object instance
       string* s = malloc(sizeof(string));
       // If "Hello" expands to string(c"Hello"), the assignment below will compile successfully
       s = "Hello";
       printf(c"String len: %d\n", s.len);

       return 0;
   }
   ```
   Compile the test with `./alkyl test_lexer.aky`. Ensure compilation succeeds and it prints `String len: 5`.
4. **Validate fallback behavior**:
   Modify `test_lexer.aky` to set `lexer.double_quote_as_string = false`:
   ```hky
   extern int printf(char* fmt, ...);
   premeta {
       lexer.double_quote_as_string = false
       reason "Testing fallback behavior"
   }

   int main() {
       // With double_quote_as_string = false, "..." behaves as char*
       char* raw = "Hello Raw C-string!";
       printf(c"Message: %s\n", raw);
       return 0;
   }
   ```
   Compile and run. Ensure it prints `Message: Hello Raw C-string!`.
