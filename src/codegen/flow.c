#include "codegen.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void push_loop_ctx(CodegenCtx *ctx, LLVMBasicBlockRef cont, LLVMBasicBlockRef brk) {
  LoopContext *lc = malloc(sizeof(LoopContext));
  lc->continue_target = cont;
  lc->break_target = brk;
  lc->parent = ctx->current_loop;
  ctx->current_loop = lc;
}

void pop_loop_ctx(CodegenCtx *ctx) {
  if (!ctx->current_loop) return;
  LoopContext *lc = ctx->current_loop;
  ctx->current_loop = lc->parent;
  free(lc);
}

void codegen_func_def(CodegenCtx *ctx, FuncDefNode *node) {
  int param_count = 0;
  Parameter *p = node->params;
  while(p) { param_count++; p = p->next; }
  
  // Method 'this' injection
  int total_params = param_count;
  if (node->class_name) total_params++;
  
  LLVMTypeRef *param_types = malloc(sizeof(LLVMTypeRef) * total_params);
  int idx = 0;
  
  if (node->class_name) {
      ClassInfo *ci = find_class(ctx, node->class_name);
      param_types[idx++] = LLVMPointerType(ci->struct_type, 0);
  }
  
  p = node->params;
  for(; idx<total_params; idx++) {
    param_types[idx] = get_llvm_type(ctx, p->type);
    p = p->next;
  }
  
  LLVMTypeRef ret_type = get_llvm_type(ctx, node->ret_type);
  LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types, total_params, node->is_varargs);
  
  // Use Mangled Name (if available) unless it is main
  const char *func_name = node->name;
  if (node->mangled_name && strcmp(node->name, "main") != 0) {
      func_name = node->mangled_name;
  }
  
  LLVMValueRef func = LLVMAddFunction(ctx->module, func_name, func_type);
  free(param_types);
  
  if (!node->body) return; // Extern

  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
  LLVMBasicBlockRef prev_block = LLVMGetInsertBlock(ctx->builder); 
  LLVMPositionBuilderAtEnd(ctx->builder, entry);
  
  Symbol *saved_scope = ctx->symbols;
  
  idx = 0;
  if (node->class_name) {
      LLVMValueRef this_val = LLVMGetParam(func, idx);
      LLVMTypeRef this_type = LLVMPointerType(find_class(ctx, node->class_name)->struct_type, 0);
      LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, this_type, "this");
      LLVMBuildStore(ctx->builder, this_val, alloca);
      
      VarType this_vt = {TYPE_CLASS, 1, strdup(node->class_name)}; // this is T*
      add_symbol(ctx, "this", alloca, this_type, this_vt, 0, 0);
      idx++;
  }
  
  p = node->params;
  for(; idx<total_params; idx++) {
    LLVMValueRef arg_val = LLVMGetParam(func, idx);
    LLVMTypeRef type = get_llvm_type(ctx, p->type);
    LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, type, p->name);
    LLVMBuildStore(ctx->builder, arg_val, alloca);
    add_symbol(ctx, p->name, alloca, type, p->type, 0, 1); 
    p = p->next;
  }
  
  codegen_node(ctx, node->body);
  
  if (node->ret_type.base == TYPE_VOID) {
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildRetVoid(ctx->builder);
    }
  } else {
     if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      // Return Safe Zero Memory Representation for everything instead of just throwing i32 0. Fixes implicit fallback validation.
      LLVMBuildRet(ctx->builder, LLVMConstNull(ret_type));
    }
  }
  
  ctx->symbols = saved_scope; 
  if (prev_block) LLVMPositionBuilderAtEnd(ctx->builder, prev_block);
}

// loop [int] {}
void codegen_loop(CodegenCtx *ctx, LoopNode *node) {
  LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
  LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlock(func, "loop_cond");
  LLVMBasicBlockRef body_bb = LLVMAppendBasicBlock(func, "loop_body");
  LLVMBasicBlockRef step_bb = LLVMAppendBasicBlock(func, "loop_step");
  LLVMBasicBlockRef end_bb = LLVMAppendBasicBlock(func, "loop_end");

  LLVMValueRef counter_ptr = NULL;
  
  if (ctx->flux_ctx_val) {
      counter_ptr = LLVMBuildAlloca(ctx->builder, LLVMInt64Type(), "loop_i");
  } else {
      counter_ptr = LLVMBuildAlloca(ctx->builder, LLVMInt64Type(), "loop_i");
  }
  
  LLVMBuildStore(ctx->builder, LLVMConstInt(LLVMInt64Type(), 0, 0), counter_ptr);
  LLVMBuildBr(ctx->builder, cond_bb);

  LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
  LLVMValueRef cur_i = LLVMBuildLoad2(ctx->builder, LLVMInt64Type(), counter_ptr, "i_val");
  LLVMValueRef limit = codegen_expr(ctx, node->iterations);
  
  if (LLVMGetTypeKind(LLVMTypeOf(limit)) != LLVMIntegerTypeKind) {
     limit = LLVMBuildFPToUI(ctx->builder, limit, LLVMInt64Type(), "limit_cast");
  } else {
     limit = LLVMBuildIntCast(ctx->builder, limit, LLVMInt64Type(), "limit_cast");
  }

  LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntULT, cur_i, limit, "cmp");
  LLVMBuildCondBr(ctx->builder, cmp, body_bb, end_bb);

  LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
  push_loop_ctx(ctx, step_bb, end_bb);
  codegen_node(ctx, node->body);
  pop_loop_ctx(ctx);
  
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildBr(ctx->builder, step_bb);
  }

  LLVMPositionBuilderAtEnd(ctx->builder, step_bb);
  LLVMValueRef cur_i_step = LLVMBuildLoad2(ctx->builder, LLVMInt64Type(), counter_ptr, "i_val_step");
  LLVMValueRef next_i = LLVMBuildAdd(ctx->builder, cur_i_step, LLVMConstInt(LLVMInt64Type(), 1, 0), "next_i");
  LLVMBuildStore(ctx->builder, next_i, counter_ptr);
  LLVMBuildBr(ctx->builder, cond_bb);

  LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
}

// while or while once
void codegen_while(CodegenCtx *ctx, WhileNode *node) {
  LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
  LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlock(func, "while_cond");
  LLVMBasicBlockRef body_bb = LLVMAppendBasicBlock(func, "while_body");
  LLVMBasicBlockRef end_bb = LLVMAppendBasicBlock(func, "while_end");

  if (node->is_do_while) {
      LLVMBuildBr(ctx->builder, body_bb);
      LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
      push_loop_ctx(ctx, cond_bb, end_bb);
      codegen_node(ctx, node->body);
      pop_loop_ctx(ctx);

      if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
          LLVMBuildBr(ctx->builder, cond_bb);
      }
      LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
      LLVMValueRef cond = codegen_expr(ctx, node->condition);
      if (LLVMGetTypeKind(LLVMTypeOf(cond)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(cond)) != 1) {
          cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, 0), "to_bool");
      }
      LLVMBuildCondBr(ctx->builder, cond, body_bb, end_bb);
      
  } else {
      LLVMBuildBr(ctx->builder, cond_bb);
      LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
      
      LLVMValueRef cond = codegen_expr(ctx, node->condition);
      if (LLVMGetTypeKind(LLVMTypeOf(cond)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(cond)) != 1) {
          cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, 0), "to_bool");
      }
      LLVMBuildCondBr(ctx->builder, cond, body_bb, end_bb);

      LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
      push_loop_ctx(ctx, cond_bb, end_bb);
      codegen_node(ctx, node->body);
      pop_loop_ctx(ctx);
      
      if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
          LLVMBuildBr(ctx->builder, cond_bb);
      }
  }

  LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
}

// switch ()
void codegen_switch(CodegenCtx *ctx, SwitchNode *node) {
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMValueRef cond = codegen_expr(ctx, node->condition);
    
    if (LLVMGetTypeKind(LLVMTypeOf(cond)) != LLVMIntegerTypeKind) {
        cond = LLVMBuildIntCast(ctx->builder, cond, LLVMInt32Type(), "switch_cond_cast");
    }

    LLVMBasicBlockRef end_bb = LLVMAppendBasicBlock(func, "switch_end");
    LLVMBasicBlockRef default_bb = LLVMAppendBasicBlock(func, "switch_default");
    
    int case_count = 0;
    ASTNode *c = node->cases;
    while(c) { case_count++; c = c->next; }
    
    LLVMBasicBlockRef *case_bbs = malloc(sizeof(LLVMBasicBlockRef) * case_count);
    for(int i=0; i<case_count; i++) {
        case_bbs[i] = LLVMAppendBasicBlock(func, "case_bb");
    }
    
    LLVMValueRef switch_inst = LLVMBuildSwitch(ctx->builder, cond, default_bb, case_count);
    
    c = node->cases;
    int i = 0;
    while(c) {
        CaseNode *cn = (CaseNode*)c;
        LLVMValueRef val = codegen_expr(ctx, cn->value);
        if (LLVMTypeOf(val) != LLVMTypeOf(cond)) {
            if (LLVMIsConstant(val) && LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMIntegerTypeKind) {
                unsigned long long raw_val = LLVMConstIntGetZExtValue(val);
                val = LLVMConstInt(LLVMTypeOf(cond), raw_val, 0);
            } else {
                val = LLVMConstBitCast(val, LLVMTypeOf(cond));
            }
        }
        
        LLVMAddCase(switch_inst, val, case_bbs[i]);
        
        LLVMPositionBuilderAtEnd(ctx->builder, case_bbs[i]);
        push_loop_ctx(ctx, NULL, end_bb); 
        codegen_node(ctx, cn->body);
        pop_loop_ctx(ctx);
        
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
            if (cn->is_leak) {
                if (i + 1 < case_count) {
                    LLVMBuildBr(ctx->builder, case_bbs[i+1]);
                } else {
                    LLVMBuildBr(ctx->builder, default_bb);
                }
            } else {
                LLVMBuildBr(ctx->builder, end_bb);
            }
        }
        
        c = c->next;
        i++;
    }
    free(case_bbs);
    
    LLVMPositionBuilderAtEnd(ctx->builder, default_bb);
    if (node->default_case) {
        push_loop_ctx(ctx, NULL, end_bb);
        codegen_node(ctx, node->default_case);
        pop_loop_ctx(ctx);
    }
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
        LLVMBuildBr(ctx->builder, end_bb);
    }
    
    LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
}

void codegen_break(CodegenCtx *ctx) {
    if (!ctx->current_loop) {
        fprintf(stderr, "Error: 'break' outside of loop or switch\n");
    }
    LLVMBuildBr(ctx->builder, ctx->current_loop->break_target);
}

void codegen_continue(CodegenCtx *ctx) {
    if (!ctx->current_loop || !ctx->current_loop->continue_target) {
        fprintf(stderr, "Error: 'continue' outside of loop\n");
    }
    LLVMBuildBr(ctx->builder, ctx->current_loop->continue_target);
}

void codegen_if(CodegenCtx *ctx, IfNode *node) {
  LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
  LLVMBasicBlockRef then_bb = LLVMAppendBasicBlock(func, "if_then");
  LLVMBasicBlockRef else_bb = LLVMAppendBasicBlock(func, "if_else");
  LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlock(func, "if_merge");

  LLVMValueRef cond = codegen_expr(ctx, node->condition);
  if (LLVMGetTypeKind(LLVMTypeOf(cond)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(cond)) != 1) {
    cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, 0), "to_bool");
  }
  
  LLVMBuildCondBr(ctx->builder, cond, then_bb, else_bb);

  LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
  codegen_node(ctx, node->then_body);
  if (!LLVMGetBasicBlockTerminator(then_bb)) LLVMBuildBr(ctx->builder, merge_bb);

  LLVMPositionBuilderAtEnd(ctx->builder, else_bb);
  if (node->else_body) codegen_node(ctx, node->else_body);
  if (!LLVMGetBasicBlockTerminator(else_bb)) LLVMBuildBr(ctx->builder, merge_bb);

  LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

// --- FLUX CODE GENERATION ---

// Helper to collect variable declarations in the flux body
void collect_flux_vars(ASTNode *node, LLVMTypeRef *types, VarType *vtypes, char **names, int *count, int *cap, CodegenCtx *ctx) {
    if (!node) return;
    
    if (node->type == NODE_VAR_DECL) {
        VarDeclNode *vd = (VarDeclNode*)node;
        if (*count >= *cap) {
            *cap *= 2;
            types = realloc(types, sizeof(LLVMTypeRef) * (*cap));
            vtypes = realloc(vtypes, sizeof(VarType) * (*cap));
            names = realloc(names, sizeof(char*) * (*cap));
        }
        
        // Determine type using codegen calculation if AUTO
        VarType final_vt = vd->var_type;
        if (final_vt.base == TYPE_AUTO && vd->initializer) {
            final_vt = codegen_calc_type(ctx, vd->initializer);
        }
        
        LLVMTypeRef t = get_llvm_type(ctx, final_vt);

        if (vd->is_array) {
            int sz = 10;
            if (vd->array_size && vd->array_size->type == NODE_LITERAL) {
                sz = ((LiteralNode*)vd->array_size)->val.int_val;
            } else if (vd->initializer && vd->initializer->type == NODE_ARRAY_LIT) {
                // Infer size from initializer
                ArrayLitNode *lit = (ArrayLitNode*)vd->initializer;
                int c = 0; ASTNode *el = lit->elements;
                while(el) { c++; el = el->next; }
                sz = c;
            }
            final_vt.array_size = sz;
            t = LLVMArrayType(t, sz);
        }

        names[*count] = strdup(vd->name);
        types[*count] = t;
        vtypes[*count] = final_vt;
        (*count)++;
    }
    
    // Recurse children
    if (node->type == NODE_IF) {
        collect_flux_vars(((IfNode*)node)->then_body, types, vtypes, names, count, cap, ctx);
        collect_flux_vars(((IfNode*)node)->else_body, types, vtypes, names, count, cap, ctx);
    } else if (node->type == NODE_WHILE) {
        collect_flux_vars(((WhileNode*)node)->body, types, vtypes, names, count, cap, ctx);
    } else if (node->type == NODE_LOOP) {
        collect_flux_vars(((LoopNode*)node)->body, types, vtypes, names, count, cap, ctx);
    } else if (node->type == NODE_FOR_IN) {
        // Handle iterator variable
        ForInNode *f = (ForInNode*)node;
        if (*count >= *cap) {
            *cap *= 2;
            types = realloc(types, sizeof(LLVMTypeRef) * (*cap));
            vtypes = realloc(vtypes, sizeof(VarType) * (*cap));
            names = realloc(names, sizeof(char*) * (*cap));
        }
        names[*count] = strdup(f->var_name);
        types[*count] = get_llvm_type(ctx, f->iter_type);
        vtypes[*count] = f->iter_type;
        (*count)++;
        
        collect_flux_vars(f->body, types, vtypes, names, count, cap, ctx);
    } else if (node->type == NODE_SWITCH) {
        SwitchNode *sw = (SwitchNode*)node;
        ASTNode *c = sw->cases;
        while(c) {
            collect_flux_vars(((CaseNode*)c)->body, types, vtypes, names, count, cap, ctx);
            c = c->next;
        }
        if (sw->default_case) {
            collect_flux_vars(sw->default_case, types, vtypes, names, count, cap, ctx);
        }
    }

    collect_flux_vars(node->next, types, vtypes, names, count, cap, ctx);
}

// Helper: Transform NODE_RETURN into NODE_BREAK inside flux
void replace_returns_with_breaks(ASTNode *node) {
    while (node) {
        if (node->type == NODE_RETURN) {
            node->type = NODE_BREAK;
        }
        
        if (node->type == NODE_IF) {
            replace_returns_with_breaks(((IfNode*)node)->then_body);
            replace_returns_with_breaks(((IfNode*)node)->else_body);
        } else if (node->type == NODE_WHILE) {
            replace_returns_with_breaks(((WhileNode*)node)->body);
        } else if (node->type == NODE_LOOP) {
            replace_returns_with_breaks(((LoopNode*)node)->body);
        } else if (node->type == NODE_FOR_IN) {
            replace_returns_with_breaks(((ForInNode*)node)->body);
        } else if (node->type == NODE_SWITCH) {
            ASTNode *c = ((SwitchNode*)node)->cases;
            while(c) {
                replace_returns_with_breaks(((CaseNode*)c)->body);
                c = c->next;
            }
            replace_returns_with_breaks(((SwitchNode*)node)->default_case);
        }
        
        node = node->next;
    }
}

// Helper: Rewrite VarDecls to Assignments in Flux Body
ASTNode* rewrite_decls_to_assigns(ASTNode *node) {
    if (!node) return NULL;
    
    node->next = rewrite_decls_to_assigns(node->next);
    
    if (node->type == NODE_VAR_DECL) {
        VarDeclNode *vd = (VarDeclNode*)node;
        
        if (vd->initializer) {
            AssignNode *an = calloc(1, sizeof(AssignNode));
            an->base.type = NODE_ASSIGN;
            an->base.next = node->next;
            an->base.line = node->line;
            an->base.col = node->col;
            an->name = strdup(vd->name);
            an->value = vd->initializer;
            an->op = TOKEN_ASSIGN;
            
            return (ASTNode*)an;
        } else {
            return node->next;
        }
    }
    
    if (node->type == NODE_IF) {
        ((IfNode*)node)->then_body = rewrite_decls_to_assigns(((IfNode*)node)->then_body);
        ((IfNode*)node)->else_body = rewrite_decls_to_assigns(((IfNode*)node)->else_body);
    } else if (node->type == NODE_WHILE) {
        ((WhileNode*)node)->body = rewrite_decls_to_assigns(((WhileNode*)node)->body);
    } else if (node->type == NODE_LOOP) {
        ((LoopNode*)node)->body = rewrite_decls_to_assigns(((LoopNode*)node)->body);
    } else if (node->type == NODE_FOR_IN) {
        ((ForInNode*)node)->body = rewrite_decls_to_assigns(((ForInNode*)node)->body);
    } else if (node->type == NODE_SWITCH) {
        SwitchNode *sw = (SwitchNode*)node;
        ASTNode *c = sw->cases;
        while(c) {
             ((CaseNode*)c)->body = rewrite_decls_to_assigns(((CaseNode*)c)->body);
             c = c->next;
        }
        sw->default_case = rewrite_decls_to_assigns(sw->default_case);
    }
    
    return node;
}

void codegen_flux_def(CodegenCtx *ctx, FuncDefNode *node) {
    // 1. Define the Context Struct
    int param_count = 0;
    Parameter *p = node->params;
    while(p) { param_count++; p = p->next; }
    
    // Temporarily add params to symbol table so collect_flux_vars can infer types of locals
    Symbol *saved_pre_scan_syms = ctx->symbols;
    p = node->params;
    for(int i=0; i<param_count; i++) {
        // We don't have LLVM values yet, but we need symbols for codegen_calc_type to work.
        // We can pass NULL as value since calc_type only needs vtype.
        add_symbol(ctx, p->name, NULL, NULL, p->type, 0, 1);
        p = p->next;
    }

    // Locals scan
    int local_cap = 16;
    int local_count = 0;
    LLVMTypeRef *local_types = malloc(sizeof(LLVMTypeRef) * local_cap);
    VarType *local_vtypes = malloc(sizeof(VarType) * local_cap);
    char **local_names = malloc(sizeof(char*) * local_cap);
    
    collect_flux_vars(node->body, local_types, local_vtypes, local_names, &local_count, &local_cap, ctx);
    
    // Restore symbols (remove params) to clean state before actual codegen
    ctx->symbols = saved_pre_scan_syms;

    // Total fields: 1 (state) + params + locals
    int total_fields = 1 + param_count + local_count;
    LLVMTypeRef *struct_elems = malloc(sizeof(LLVMTypeRef) * total_fields);
    
    struct_elems[0] = LLVMInt32Type(); // State
    
    // Params
    p = node->params;
    for(int i=0; i<param_count; i++) {
        struct_elems[1+i] = get_llvm_type(ctx, p->type);
        p = p->next;
    }
    
    // Locals
    for(int i=0; i<local_count; i++) {
        struct_elems[1+param_count+i] = local_types[i];
    }
    
    char struct_name[256];
    snprintf(struct_name, 256, "FluxCtx_%s", node->name);
    
    LLVMTypeRef ctx_type = LLVMGetTypeByName(ctx->module, struct_name);
    if (!ctx_type) {
        ctx_type = LLVMStructCreateNamed(LLVMGetGlobalContext(), struct_name);
    }
    LLVMStructSetBody(ctx_type, struct_elems, total_fields, false);
    
    // CRITICAL FIX: Register the struct type as a ClassInfo so get_llvm_type 
    // can find it by name if it's used elsewhere (e.g. by implicit inference),
    // preventing creation of opaque collision types (.0)
    if (!find_class(ctx, struct_name)) {
        ClassInfo *ci = calloc(1, sizeof(ClassInfo));
        ci->name = strdup(struct_name);
        ci->struct_type = ctx_type;
        add_class_info(ctx, ci);
    }

    ctx->current_flux_struct_type = ctx_type;
    
    // 2. Generate the Init/Factory Function
    LLVMTypeRef *init_param_types = malloc(sizeof(LLVMTypeRef) * param_count);
    p = node->params;
    for(int i=0; i<param_count; i++) {
        init_param_types[i] = get_llvm_type(ctx, p->type);
        p = p->next;
    }
    
    LLVMTypeRef init_func_type = LLVMFunctionType(LLVMPointerType(ctx_type, 0), init_param_types, param_count, false);
    LLVMValueRef init_func = LLVMAddFunction(ctx->module, node->name, init_func_type);
    
    LLVMBasicBlockRef init_entry = LLVMAppendBasicBlock(init_func, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, init_entry);
    
    LLVMValueRef size = LLVMSizeOf(ctx_type);
    LLVMValueRef mem = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(ctx->malloc_func), ctx->malloc_func, &size, 1, "ctx_mem");
    LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, mem, LLVMPointerType(ctx_type, 0), "ctx");
    
    LLVMValueRef state_ptr = LLVMBuildStructGEP2(ctx->builder, ctx_type, ctx_ptr, 0, "state_ptr");
    LLVMBuildStore(ctx->builder, LLVMConstInt(LLVMInt32Type(), 0, 0), state_ptr);
    
    for(int i=0; i<param_count; i++) {
        LLVMValueRef arg = LLVMGetParam(init_func, i);
        LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder, ctx_type, ctx_ptr, 1+i, "param_ptr");
        LLVMBuildStore(ctx->builder, arg, field_ptr);
    }
    
    LLVMBuildRet(ctx->builder, ctx_ptr);
    
    // 3. Generate the Next Function
    LLVMTypeRef yield_type = get_llvm_type(ctx, node->ret_type);
    LLVMTypeRef res_struct_elems[] = { LLVMInt1Type(), yield_type };
    LLVMTypeRef res_type = LLVMStructType(res_struct_elems, 2, false);
    
    char next_func_name[256];
    snprintf(next_func_name, 256, "%s_next", node->name);
    
    LLVMTypeRef next_args[] = { LLVMPointerType(ctx_type, 0) };
    LLVMTypeRef next_func_type = LLVMFunctionType(res_type, next_args, 1, false);
    LLVMValueRef next_func = LLVMAddFunction(ctx->module, next_func_name, next_func_type);
    
    LLVMBasicBlockRef next_entry = LLVMAppendBasicBlock(next_func, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, next_entry);
    
    LLVMValueRef ctx_arg = LLVMGetParam(next_func, 0);
    ctx->flux_ctx_val = ctx_arg;
    
    LLVMValueRef current_state_ptr = LLVMBuildStructGEP2(ctx->builder, ctx_type, ctx_arg, 0, "state_ptr");
    LLVMValueRef current_state = LLVMBuildLoad2(ctx->builder, LLVMInt32Type(), current_state_ptr, "state");
    
    Symbol *saved_syms = ctx->symbols;
    
    p = node->params;
    for(int i=0; i<param_count; i++) {
        LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder, ctx_type, ctx_arg, 1+i, p->name);
        add_symbol(ctx, p->name, field_ptr, get_llvm_type(ctx, p->type), p->type, 0, 1);
        p = p->next;
    }
    
    for(int i=0; i<local_count; i++) {
        LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder, ctx_type, ctx_arg, 1+param_count+i, local_names[i]);
        add_symbol(ctx, local_names[i], field_ptr, local_types[i], local_vtypes[i], 0, 1);
    }
    
    LLVMBasicBlockRef start_bb = LLVMAppendBasicBlock(next_func, "start_logic");
    LLVMBasicBlockRef default_bb = LLVMAppendBasicBlock(next_func, "finished");
    
    LLVMValueRef switch_inst = LLVMBuildSwitch(ctx->builder, current_state, default_bb, 10);
    LLVMAddCase(switch_inst, LLVMConstInt(LLVMInt32Type(), 0, 0), start_bb);
    ctx->current_switch_inst = switch_inst;
    ctx->next_flux_state = 1;

    replace_returns_with_breaks(node->body);
    node->body = rewrite_decls_to_assigns(node->body);

    push_loop_ctx(ctx, default_bb, default_bb);
    LLVMPositionBuilderAtEnd(ctx->builder, start_bb);
    codegen_node(ctx, node->body);
    pop_loop_ctx(ctx);

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
        LLVMBuildBr(ctx->builder, default_bb);
    }
    
    LLVMPositionBuilderAtEnd(ctx->builder, default_bb);
    LLVMValueRef undef_ret = LLVMGetUndef(res_type);
    LLVMValueRef ret_0 = LLVMBuildInsertValue(ctx->builder, undef_ret, LLVMConstInt(LLVMInt1Type(), 0, 0), 0, "set_valid");
    LLVMBuildRet(ctx->builder, ret_0);

    ctx->symbols = saved_syms;
    ctx->current_switch_inst = NULL;
    ctx->flux_ctx_val = NULL;
    ctx->current_flux_struct_type = NULL;
    
    free(struct_elems);
    free(init_param_types);
    free(local_types);
    free(local_vtypes);
    free(local_names);
}

void codegen_emit(CodegenCtx *ctx, EmitNode *node) {
    if (!ctx->current_switch_inst) {
        codegen_error(ctx, (ASTNode*)node, "emit used outside of flux function");
    }

    LLVMValueRef val = codegen_expr(ctx, node->value);
    int next_state = ctx->next_flux_state++;
    
    LLVMValueRef ctx_ptr = ctx->flux_ctx_val;
    if (!ctx->current_flux_struct_type) {
         codegen_error(ctx, (ASTNode*)node, "Internal Error: Emit used without flux struct type context");
    }
    LLVMTypeRef ctx_type = ctx->current_flux_struct_type;
    
    LLVMValueRef state_ptr = LLVMBuildStructGEP2(ctx->builder, ctx_type, ctx_ptr, 0, "state_ptr");
    LLVMBuildStore(ctx->builder, LLVMConstInt(LLVMInt32Type(), next_state, 0), state_ptr);
    
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMTypeRef res_type = LLVMGetReturnType(LLVMGlobalGetValueType(func));
    
    LLVMValueRef undef = LLVMGetUndef(res_type);
    LLVMValueRef res_1 = LLVMBuildInsertValue(ctx->builder, undef, LLVMConstInt(LLVMInt1Type(), 1, 0), 0, "set_valid");
    LLVMValueRef res_2 = LLVMBuildInsertValue(ctx->builder, res_1, val, 1, "set_val");
    
    LLVMBuildRet(ctx->builder, res_2);
    
    LLVMBasicBlockRef resume_bb = LLVMAppendBasicBlock(func, "resume");
    LLVMAddCase(ctx->current_switch_inst, LLVMConstInt(LLVMInt32Type(), next_state, 0), resume_bb);
    LLVMPositionBuilderAtEnd(ctx->builder, resume_bb);
}

void codegen_for_in(CodegenCtx *ctx, ForInNode *node) {
    LLVMValueRef col = codegen_expr(ctx, node->collection);
    VarType col_type = codegen_calc_type(ctx, node->collection);
    
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlock(func, "for_cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlock(func, "for_body");
    LLVMBasicBlockRef end_bb = LLVMAppendBasicBlock(func, "for_end");
    
    LLVMValueRef iter_ptr = NULL;
    LLVMValueRef flux_ctx = NULL;
    
    if (col_type.base == TYPE_STRING || (col_type.base == TYPE_CHAR && col_type.ptr_depth == 1)) {
        iter_ptr = LLVMBuildAlloca(ctx->builder, LLVMPointerType(LLVMInt8Type(), 0), "str_iter");
        LLVMBuildStore(ctx->builder, col, iter_ptr);
    } else if (col_type.base == TYPE_INT && col_type.array_size == 0 && col_type.ptr_depth == 0) {
        iter_ptr = LLVMBuildAlloca(ctx->builder, LLVMInt64Type(), "range_i");
        LLVMBuildStore(ctx->builder, LLVMConstInt(LLVMInt64Type(), 0, 0), iter_ptr);
    } else {
        flux_ctx = col;
    }
    
    LLVMBuildBr(ctx->builder, cond_bb);
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    
    LLVMValueRef current_val = NULL;
    LLVMValueRef condition = NULL;
    
    if (flux_ctx) {
        char next_name[256];
        int found = 0;
        
        if (node->collection->type == NODE_CALL) {
             CallNode* cn = (CallNode*)node->collection;
             snprintf(next_name, 256, "%s_next", cn->name);
             found = 1;
        } else {
             LLVMTypeRef ptr_t = LLVMTypeOf(flux_ctx);
             if (LLVMGetTypeKind(ptr_t) == LLVMPointerTypeKind) {
                 LLVMTypeRef el_t = LLVMGetElementType(ptr_t);
                 if (LLVMGetTypeKind(el_t) == LLVMStructTypeKind) {
                     const char *sname = LLVMGetStructName(el_t);
                     if (sname && strncmp(sname, "FluxCtx_", 8) == 0) {
                         snprintf(next_name, 256, "%s_next", sname + 8);
                         found = 1;
                     }
                 }
             }
        }
        
        if (!found) snprintf(next_name, 256, "UnknownFlux_next");

        LLVMValueRef next_func = LLVMGetNamedFunction(ctx->module, next_name);
        if (!next_func) {
             codegen_error(ctx, (ASTNode*)node, "Could not find flux next function.");
        }
        
        LLVMTypeRef func_t = LLVMGlobalGetValueType(next_func);
        LLVMTypeRef expected_ptr_t = LLVMTypeOf(LLVMGetParam(next_func, 0));
        
        if (LLVMTypeOf(flux_ctx) != expected_ptr_t) {
            flux_ctx = LLVMBuildBitCast(ctx->builder, flux_ctx, expected_ptr_t, "ctx_cast");
        }

        LLVMValueRef res = LLVMBuildCall2(ctx->builder, func_t, next_func, &flux_ctx, 1, "res");
        condition = LLVMBuildExtractValue(ctx->builder, res, 0, "is_valid");
        current_val = LLVMBuildExtractValue(ctx->builder, res, 1, "val");
    } 
    else if (iter_ptr && col_type.base == TYPE_INT) {
        LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, LLVMInt64Type(), iter_ptr, "idx");
        LLVMValueRef limit = LLVMBuildIntCast(ctx->builder, col, LLVMInt64Type(), "limit");
        condition = LLVMBuildICmp(ctx->builder, LLVMIntSLT, idx, limit, "chk");
        current_val = LLVMBuildIntCast(ctx->builder, idx, LLVMInt32Type(), "val");
    }
    else {
        LLVMValueRef p = LLVMBuildLoad2(ctx->builder, LLVMPointerType(LLVMInt8Type(), 0), iter_ptr, "p");
        LLVMValueRef c = LLVMBuildLoad2(ctx->builder, LLVMInt8Type(), p, "char");
        condition = LLVMBuildICmp(ctx->builder, LLVMIntNE, c, LLVMConstInt(LLVMInt8Type(), 0, 0), "chk");
        current_val = c;
    }
    
    LLVMBuildCondBr(ctx->builder, condition, body_bb, end_bb);
    
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    
    LLVMTypeRef var_type = get_llvm_type(ctx, node->iter_type);
    LLVMValueRef var_alloca = NULL;
    int is_new_var = 1;
    
    if (ctx->flux_ctx_val) {
        Symbol *existing = find_symbol(ctx, node->var_name);
        if (existing) {
            var_alloca = existing->value;
            is_new_var = 0;
        }
    }
    
    if (!var_alloca) {
        var_alloca = LLVMBuildAlloca(ctx->builder, var_type, node->var_name);
    }
    
    LLVMBuildStore(ctx->builder, current_val, var_alloca);
    
    Symbol *saved_syms = ctx->symbols;
    if (is_new_var) {
        add_symbol(ctx, node->var_name, var_alloca, var_type, node->iter_type, 0, 0);
    }
    
    push_loop_ctx(ctx, cond_bb, end_bb);
    codegen_node(ctx, node->body);
    pop_loop_ctx(ctx);
    
    if (flux_ctx) {
        // Step in check
    } else if (iter_ptr && col_type.base == TYPE_INT) {
        LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, LLVMInt64Type(), iter_ptr, "idx");
        LLVMValueRef nxt = LLVMBuildAdd(ctx->builder, idx, LLVMConstInt(LLVMInt64Type(), 1, 0), "inc");
        LLVMBuildStore(ctx->builder, nxt, iter_ptr);
    } else {
        LLVMValueRef p = LLVMBuildLoad2(ctx->builder, LLVMPointerType(LLVMInt8Type(), 0), iter_ptr, "p");
        LLVMValueRef nxt = LLVMBuildGEP2(ctx->builder, LLVMInt8Type(), p, (LLVMValueRef[]){LLVMConstInt(LLVMInt64Type(), 1, 0)}, 1, "inc");
        LLVMBuildStore(ctx->builder, nxt, iter_ptr);
    }
    
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
        LLVMBuildBr(ctx->builder, cond_bb);
    }
    
    ctx->symbols = saved_syms;
    LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
}
