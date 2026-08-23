--- src/alir/fragment/generate.c
+++ src/alir/fragment/generate.c
@@ -165,7 +165,7 @@
         if (val) {
             if (vn->var_type.base == TYPE_AUTO || vn->var_type.base == TYPE_UNKNOWN) {
                 vn->var_type = val->type;
-            } else if (val->type.ptr_depth > vn->var_type.ptr_depth) {
+            } else if (val->type.ptr_depth > vn->var_type.ptr_depth && !vn->var_type.is_tainted) {
                 vn->var_type = val->type;
             } else if (vn->var_type.base == TYPE_CLASS && val->type.base == TYPE_CLASS) {
                 vn->var_type.ptr_depth = val->type.ptr_depth;
