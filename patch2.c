--- src/alir/utils.c
+++ src/alir/utils.c
@@ -128,6 +128,10 @@
         }
     }
 
-    return obj_ptr;
+    VarType cls_val_type = {TYPE_CLASS, 0, alir_strdup(ctx->module, class_name)};
+    AlirValue *obj_val = new_temp(ctx, cls_val_type);
+    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, obj_val, obj_ptr, NULL));
+
+    return obj_val;
 }
