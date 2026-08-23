--- src/alir/fragment/generate.c
+++ src/alir/fragment/generate.c
@@ -173,7 +173,29 @@
             if (val->type.is_tainted) {
                 vn->var_type.is_tainted = 1;
             }
-            val = promote(ctx, val, vn->var_type);
+            
+            if (vn->var_type.is_tainted && val->type.base == TYPE_VOID && val->type.ptr_depth == 1 && val->kind == ALIR_VAL_INT && val->val_int == 0) {
+                int err_id = 1;
+                if (ctx->sem && ctx->sem->compiler_ctx) {
+                    void *val_err = hashmap_get(&ctx->sem->compiler_ctx->error_table, "ErrNull");
+                    if (val_err) err_id = (int)(intptr_t)val_err;
+                }
+                AlirValue *ptr2 = new_temp(ctx, vn->var_type);
+                emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr2, NULL, NULL));
+                alir_add_symbol(ctx, vn->name, ptr2, vn->var_type);
+                
+                AlirValue *err_ptr = new_temp(ctx, (VarType){TYPE_INT, 1, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
+                emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, err_ptr, ptr2, alir_const_int(ctx->module, 0)));
+                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, alir_const_int(ctx->module, err_id), err_ptr));
+                
+                AlirValue *val_ptr = new_temp(ctx, (VarType){TYPE_INT, 1, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0}); // Type doesn't matter much for GET_PTR
+                emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, val_ptr, ptr2, alir_const_int(ctx->module, 1)));
+                
+                AlirValue *zero_val = alir_const_int(ctx->module, 0);
+                zero_val = promote(ctx, zero_val, (VarType){vn->var_type.base, vn->var_type.ptr_depth, vn->var_type.class_name, 0, 0, NULL, NULL, 0, 0, 0, 0});
+                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, zero_val, val_ptr));
+                return;
+            } else {
+                val = promote(ctx, val, vn->var_type);
+            }
         }
     }
 
