#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void codegen_assign(CodegenCtx *ctx, AssignNode *node) {
  Symbol *sym = find_symbol(ctx, node->name);
  if (!sym) { fprintf(stderr, "Error: Assignment to undefined variable %s\n", node->name); exit(1); }
  
  if (!sym->is_mutable) {
      fprintf(stderr, "Error: Assignment to immutable variable %s\n", node->name);
      exit(1);
  }

  LLVMValueRef val = codegen_expr(ctx, node->value);

  if (node->index) {
      // Array assignment
      if (!sym->is_array && LLVMGetTypeKind(sym->type) != LLVMPointerTypeKind) { 
          fprintf(stderr, "Error: Indexing non-array/non-pointer %s\n", node->name); exit(1); 
      }
      
      LLVMValueRef idx = codegen_expr(ctx, node->index);
      if (LLVMGetTypeKind(LLVMTypeOf(idx)) != LLVMIntegerTypeKind) {
         idx = LLVMBuildFPToUI(ctx->builder, idx, LLVMInt64Type(), "idx_cast");
      } else {
         idx = LLVMBuildIntCast(ctx->builder, idx, LLVMInt64Type(), "idx_cast");
      }

      LLVMValueRef ptr;
      if (sym->is_array) {
          LLVMValueRef indices[] = { LLVMConstInt(LLVMInt64Type(), 0, 0), idx };
          ptr = LLVMBuildGEP2(ctx->builder, sym->type, sym->value, indices, 2, "elem_ptr");
      } else {
          LLVMValueRef base = LLVMBuildLoad2(ctx->builder, sym->type, sym->value, "ptr_base");
          LLVMValueRef indices[] = { idx };
          ptr = LLVMBuildGEP2(ctx->builder, LLVMGetElementType(sym->type), base, indices, 1, "ptr_elem");
      }
      
      LLVMBuildStore(ctx->builder, val, ptr);

  } else {
      LLVMBuildStore(ctx->builder, val, sym->value);
  }
}

void codegen_var_decl(CodegenCtx *ctx, VarDeclNode *node) {
  LLVMValueRef alloca = NULL;
  LLVMTypeRef type = NULL;

  if (node->is_array) {
      LLVMTypeRef elem_type = (node->var_type == VAR_AUTO) ? LLVMInt32Type() : get_llvm_type(node->var_type);
      
      int size = 0;
      if (node->array_size) {
          if (node->array_size->type == NODE_LITERAL) {
             size = ((LiteralNode*)node->array_size)->val.int_val;
          } else {
              size = 10; 
          }
      } else {
          if (node->initializer && node->initializer->type == NODE_ARRAY_LIT) {
             ASTNode *el = ((ArrayLitNode*)node->initializer)->elements;
             while(el) { size++; el = el->next; }
          } else if (node->initializer && node->initializer->type == NODE_LITERAL && ((LiteralNode*)node->initializer)->var_type == VAR_STRING) {
              size = strlen(((LiteralNode*)node->initializer)->val.str_val) + 1;
              elem_type = LLVMInt8Type();
          } else {
             fprintf(stderr, "Error: Array size unknown\n"); exit(1); 
          }
      }
      
      type = LLVMArrayType(elem_type, size);
      alloca = LLVMBuildAlloca(ctx->builder, type, node->name);
      
      if (node->initializer) {
          if (node->initializer->type == NODE_ARRAY_LIT) {
             ASTNode *el = ((ArrayLitNode*)node->initializer)->elements;
             int idx = 0;
             while(el) {
                 LLVMValueRef val = codegen_expr(ctx, el);
                 LLVMValueRef indices[] = { LLVMConstInt(LLVMInt64Type(), 0, 0), LLVMConstInt(LLVMInt64Type(), idx, 0) };
                 LLVMValueRef ptr = LLVMBuildGEP2(ctx->builder, type, alloca, indices, 2, "init_ptr");
                 LLVMBuildStore(ctx->builder, val, ptr);
                 idx++;
                 el = el->next;
             }
          } else if (node->initializer->type == NODE_LITERAL && ((LiteralNode*)node->initializer)->var_type == VAR_STRING) {
             char *str = ((LiteralNode*)node->initializer)->val.str_val;
             for (int i = 0; i < size; i++) {
                 LLVMValueRef val = LLVMConstInt(LLVMInt8Type(), str[i], 0); 
                 LLVMValueRef indices[] = { LLVMConstInt(LLVMInt64Type(), 0, 0), LLVMConstInt(LLVMInt64Type(), i, 0) };
                 LLVMValueRef ptr = LLVMBuildGEP2(ctx->builder, type, alloca, indices, 2, "str_init_ptr");
                 LLVMBuildStore(ctx->builder, val, ptr);
             }
          } else {
             LLVMValueRef val = codegen_expr(ctx, node->initializer);
             LLVMTypeRef val_type = LLVMTypeOf(val);
             
             if (LLVMGetTypeKind(val_type) == LLVMPointerTypeKind && 
                 LLVMGetTypeKind(elem_type) == LLVMIntegerTypeKind && 
                 LLVMGetIntTypeWidth(elem_type) == 8) {
                 
                 LLVMValueRef strcpy_func = LLVMGetNamedFunction(ctx->module, "strcpy");
                 if (!strcpy_func) {
                     LLVMTypeRef args[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
                     LLVMTypeRef ftype = LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0), args, 2, false);
                     strcpy_func = LLVMAddFunction(ctx->module, "strcpy", ftype);
                 }
                 
                 LLVMValueRef indices[] = { LLVMConstInt(LLVMInt64Type(), 0, 0), LLVMConstInt(LLVMInt64Type(), 0, 0) };
                 LLVMValueRef dest = LLVMBuildGEP2(ctx->builder, type, alloca, indices, 2, "dest_ptr");
                 
                 LLVMValueRef args[] = { dest, val };
                 LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strcpy_func), strcpy_func, args, 2, "");
             }
          }
      }
      
  } else {
      LLVMValueRef init_val = codegen_expr(ctx, node->initializer);
      
      if (node->var_type == VAR_AUTO) {
        type = LLVMTypeOf(init_val);
      } else {
        type = get_llvm_type(node->var_type);
      }
      
      alloca = LLVMBuildAlloca(ctx->builder, type, node->name);
      LLVMBuildStore(ctx->builder, init_val, alloca);
  }

  add_symbol(ctx, node->name, alloca, type, node->is_array, node->is_mutable);
}

void codegen_return(CodegenCtx *ctx, ReturnNode *node) {
  if (node->value) {
    LLVMValueRef ret = codegen_expr(ctx, node->value);
    LLVMBuildRet(ctx->builder, ret);
  } else {
    LLVMBuildRetVoid(ctx->builder);
  }
}

void codegen_node(CodegenCtx *ctx, ASTNode *node) {
  while (node) {
    if (node->type == NODE_FUNC_DEF) codegen_func_def(ctx, (FuncDefNode*)node);
    else if (node->type == NODE_RETURN) codegen_return(ctx, (ReturnNode*)node);
    else if (node->type == NODE_CALL) codegen_expr(ctx, node); 
    else if (node->type == NODE_LOOP) codegen_loop(ctx, (LoopNode*)node);
    else if (node->type == NODE_WHILE) codegen_while(ctx, (WhileNode*)node);
    else if (node->type == NODE_IF) codegen_if(ctx, (IfNode*)node);
    else if (node->type == NODE_VAR_DECL) codegen_var_decl(ctx, (VarDeclNode*)node);
    else if (node->type == NODE_ASSIGN) codegen_assign(ctx, (AssignNode*)node);
    else if (node->type == NODE_ARRAY_ACCESS) codegen_expr(ctx, node); 
    else if (node->type == NODE_LINK) { /* Ignore */ }
    node = node->next;
  }
}
