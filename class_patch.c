@@ -87,13 +87,41 @@
       if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected name after 'class', 'struct' or 'union'");
       char *class_name = parser_strdup(p, p->current_token.text);
       eat(p, TOKEN_IDENTIFIER);
 
       if (p->current_token.type == TOKEN_QUESTION) {
           is_tainted_class = 1;
           eat(p, TOKEN_QUESTION);
       }
 
+      if (p->current_token.type == TOKEN_LBRACKET || p->current_token.type == TOKEN_LT) {
+          char full_class_name[1024];
+          snprintf(full_class_name, sizeof(full_class_name), "%s", class_name);
+          TokenType end_token = (p->current_token.type == TOKEN_LBRACKET) ? TOKEN_RBRACKET : TOKEN_GT;
+          char start_char = (p->current_token.type == TOKEN_LBRACKET) ? '[' : '<';
+          char end_char = (p->current_token.type == TOKEN_LBRACKET) ? ']' : '>';
+          eat(p, p->current_token.type);
+          size_t fn_len = strlen(full_class_name);
+          if (fn_len + 1 < sizeof(full_class_name)) {
+              full_class_name[fn_len] = start_char; full_class_name[fn_len + 1] = '\0';
+          }
+          while (p->current_token.type != end_token && p->current_token.type != TOKEN_EOF) {
+              fn_len = strlen(full_class_name);
+              if (fn_len + 1 < sizeof(full_class_name)) {
+                  const char *txt = (p->current_token.text) ? p->current_token.text : ((p->current_token.type == TOKEN_NUMBER) ? "0" : token_type_to_string(p->current_token.type));
+                  snprintf(full_class_name + fn_len, sizeof(full_class_name) - fn_len, "%s", txt);
+              }
+              eat(p, p->current_token.type);
+          }
+          eat(p, end_token);
+          fn_len = strlen(full_class_name);
+          if (fn_len + 1 < sizeof(full_class_name)) {
+              full_class_name[fn_len] = end_char; full_class_name[fn_len + 1] = '\0';
+          }
+          class_name = parser_strdup(p, full_class_name);
+      }
+
       register_typename(p, class_name, 0);
 
       char *parent_name = NULL;
