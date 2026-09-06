--- src/semantic/type.c
+++ src/semantic/type.c
@@ -223,12 +223,14 @@
     } else {
         SemSymbol *sym = lookup_local_symbol(ctx, node->name);
         if (sym) {
-            sym->type = node->var_type;
+            if (sym->type.base == TYPE_AUTO || sym->type.base == TYPE_UNKNOWN) {
+                sym->type = node->var_type;
+            }
             sym->is_mutable = node->is_mutable;
             sym->is_pure = node->is_pure;
             sym->must_pure = node->has_explicit_pure;
