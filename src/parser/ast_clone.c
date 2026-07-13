#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

VarType clone_var_type(CompilerContext *ctx, VarType t, char **type_params, VarType *replace_with, int num_params) {
    if (t.base == TYPE_CLASS && t.class_name) {
        for (int i = 0; i < num_params; i++) {
            if (strcmp(t.class_name, type_params[i]) == 0) {
                VarType new_t = replace_with[i];
                new_t.ptr_depth += t.ptr_depth;
                new_t.array_depth += t.array_depth;
                new_t.vector_depth += t.vector_depth;
                return new_t;
            }
        }
    }
    VarType res = t;
    if (t.class_name) res.class_name = arena_strdup(ctx->arena, t.class_name);
    
    if (t.is_func_ptr && t.fp_ret_type) {
        res.fp_ret_type = arena_alloc(ctx->arena, sizeof(VarType));
        *res.fp_ret_type = clone_var_type(ctx, *t.fp_ret_type, type_params, replace_with, num_params);
        if (t.fp_param_types) {
            res.fp_param_types = arena_alloc(ctx->arena, sizeof(VarType) * t.fp_param_count);
            for (int i = 0; i < t.fp_param_count; i++) {
                res.fp_param_types[i] = clone_var_type(ctx, t.fp_param_types[i], type_params, replace_with, num_params);
            }
        }
    }
    return res;
}

ASTNode* ast_clone(CompilerContext *ctx, ASTNode *node, char **type_params, VarType *replace_with, int num_params) {
    if (!node) return NULL;
    
    ASTNode *clone = NULL;
    
    switch (node->type) {
        case NODE_LITERAL: {
            LiteralNode *orig = (LiteralNode*)node;
            LiteralNode *n = arena_alloc(ctx->arena, sizeof(LiteralNode));
            *n = *orig;
            n->var_type = clone_var_type(ctx, orig->var_type, type_params, replace_with, num_params);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_VAR_REF: {
            VarRefNode *orig = (VarRefNode*)node;
            VarRefNode *n = arena_alloc(ctx->arena, sizeof(VarRefNode));
            *n = *orig;
            if (orig->name) n->name = arena_strdup(ctx->arena, orig->name);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_FUNC_DEF: {
            FuncDefNode *orig = (FuncDefNode*)node;
            FuncDefNode *n = arena_alloc(ctx->arena, sizeof(FuncDefNode));
            *n = *orig;
            if (orig->name) n->name = arena_strdup(ctx->arena, orig->name);
            n->ret_type = clone_var_type(ctx, orig->ret_type, type_params, replace_with, num_params);
            
            Parameter *phead = NULL;
            Parameter **pcurr = &phead;
            Parameter *curr_orig = orig->params;
            while (curr_orig) {
                Parameter *param = arena_alloc(ctx->arena, sizeof(Parameter));
                *param = *curr_orig;
                if (param->name) param->name = arena_strdup(ctx->arena, param->name);
                param->type = clone_var_type(ctx, curr_orig->type, type_params, replace_with, num_params);
                param->next = NULL;
                *pcurr = param;
                pcurr = &param->next;
                curr_orig = curr_orig->next;
            }
            n->params = phead;
            n->body = ast_clone(ctx, orig->body, type_params, replace_with, num_params);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_BINARY_OP: {
            BinaryOpNode *orig = (BinaryOpNode*)node;
            BinaryOpNode *n = arena_alloc(ctx->arena, sizeof(BinaryOpNode));
            *n = *orig;
            n->left = ast_clone(ctx, orig->left, type_params, replace_with, num_params);
            n->right = ast_clone(ctx, orig->right, type_params, replace_with, num_params);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_UNARY_OP: {
            UnaryOpNode *orig = (UnaryOpNode*)node;
            UnaryOpNode *n = arena_alloc(ctx->arena, sizeof(UnaryOpNode));
            *n = *orig;
            n->operand = ast_clone(ctx, orig->operand, type_params, replace_with, num_params);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_RETURN: {
            ReturnNode *orig = (ReturnNode*)node;
            ReturnNode *n = arena_alloc(ctx->arena, sizeof(ReturnNode));
            *n = *orig;
            n->value = ast_clone(ctx, orig->value, type_params, replace_with, num_params);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_CALL: {
            CallNode *orig = (CallNode*)node;
            CallNode *n = arena_alloc(ctx->arena, sizeof(CallNode));
            *n = *orig;
            if (orig->name) n->name = arena_strdup(ctx->arena, orig->name);
            if (orig->mangled_name) n->mangled_name = arena_strdup(ctx->arena, orig->mangled_name);
            if (orig->target) n->target = ast_clone(ctx, orig->target, type_params, replace_with, num_params);
            n->args = ast_clone(ctx, orig->args, type_params, replace_with, num_params);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_VAR_DECL: {
            VarDeclNode *orig = (VarDeclNode*)node;
            VarDeclNode *n = arena_alloc(ctx->arena, sizeof(VarDeclNode));
            *n = *orig;
            if (orig->name) n->name = arena_strdup(ctx->arena, orig->name);
            n->var_type = clone_var_type(ctx, orig->var_type, type_params, replace_with, num_params);
            n->initializer = ast_clone(ctx, orig->initializer, type_params, replace_with, num_params);
            n->array_size = ast_clone(ctx, orig->array_size, type_params, replace_with, num_params);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_TEMPLATE_INSTANTIATION: {
            TemplateInstNode *orig = (TemplateInstNode*)node;
            TemplateInstNode *n = arena_alloc(ctx->arena, sizeof(TemplateInstNode));
            *n = *orig;
            n->target = ast_clone(ctx, orig->target, type_params, replace_with, num_params);
            
            VarType *new_types = arena_alloc(ctx->arena, sizeof(VarType) * orig->num_template_types);
            for (int i = 0; i < orig->num_template_types; i++) {
                new_types[i] = clone_var_type(ctx, orig->template_types[i], type_params, replace_with, num_params);
            }
            n->template_types = new_types;
            clone = (ASTNode*)n;
            break;
        }
        case NODE_CLASS: {
            ClassNode *orig = (ClassNode*)node;
            ClassNode *n = arena_alloc(ctx->arena, sizeof(ClassNode));
            *n = *orig;
            if (orig->name) n->name = arena_strdup(ctx->arena, orig->name);
            if (orig->parent_name) n->parent_name = arena_strdup(ctx->arena, orig->parent_name);
            
            if (orig->traits.count > 0) {
                n->traits.names = arena_alloc(ctx->arena, sizeof(char*) * orig->traits.count);
                n->traits.count = orig->traits.count;
                for (int i = 0; i < orig->traits.count; i++) {
                    n->traits.names[i] = arena_strdup(ctx->arena, orig->traits.names[i]);
                }
            }
            n->members = ast_clone(ctx, orig->members, type_params, replace_with, num_params);
            clone = (ASTNode*)n;
            break;
        }
        default: {
            break;
        }
    }
    
    if (clone) {
        if (node->reason) clone->reason = arena_strdup(ctx->arena, node->reason);
        clone->next = ast_clone(ctx, node->next, type_params, replace_with, num_params);
    }
    return clone;
}
