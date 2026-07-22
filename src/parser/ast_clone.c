#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

VarType clone_var_type(CompilerContext *ctx, VarType t, char **type_params, VarType *replace_with, int num_params, char **rename_from, char **rename_to, int num_renames) {
    if (t.base == TYPE_CLASS && t.class_name) {
        char *bracket = strchr(t.class_name, '[');
        if (bracket) {
            char base_name[256];
            int len = bracket - t.class_name;
            strncpy(base_name, t.class_name, len);
            base_name[len] = '\0';
            
            char mangled[1024];
            strcpy(mangled, base_name);
            for (int i = 0; i < num_renames; i++) {
                if (strcmp(base_name, rename_from[i]) == 0) {
                    strcpy(mangled, rename_to[i]);
                    break;
                }
            }
            
            VarType new_t = t;
            new_t.class_name = arena_strdup(ctx->arena, mangled);
            return new_t;
        }

        for (int i = 0; i < num_params; i++) {
            if (strcmp(t.class_name, type_params[i]) == 0) {
                VarType new_t = replace_with[i];
                new_t.ptr_depth += t.ptr_depth;
                new_t.array_depth += t.array_depth;
                return new_t;
            }
        }
        for (int i = 0; i < num_renames; i++) {
            if (strcmp(t.class_name, rename_from[i]) == 0) {
                VarType new_t = t;
                new_t.class_name = arena_strdup(ctx->arena, rename_to[i]);
                return new_t;
            }
        }
    }
    VarType res = t;
    if (t.class_name) res.class_name = arena_strdup(ctx->arena, t.class_name);
    
    if (t.is_func_ptr && t.fp_ret_type) {
        res.fp_ret_type = arena_alloc(ctx->arena, sizeof(VarType));
        *res.fp_ret_type = clone_var_type(ctx, *t.fp_ret_type, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
        if (t.fp_param_types) {
            res.fp_param_types = arena_alloc(ctx->arena, sizeof(VarType) * t.fp_param_count);
            for (int i = 0; i < t.fp_param_count; i++) {
                res.fp_param_types[i] = clone_var_type(ctx, t.fp_param_types[i], type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            }
        }
    }
    return res;
}

ASTNode* ast_clone(CompilerContext *ctx, ASTNode *node, char **type_params, VarType *replace_with, int num_params, char **rename_from, char **rename_to, int num_renames) {
    if (!node) return NULL;
    
    ASTNode *clone = NULL;
    
    switch (node->type) {
        case NODE_LITERAL: {
            LiteralNode *orig = (LiteralNode*)node;
            LiteralNode *n = arena_alloc(ctx->arena, sizeof(LiteralNode));
            *n = *orig;
            n->var_type = clone_var_type(ctx, orig->var_type, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_VAR_REF: {
            VarRefNode *orig = (VarRefNode*)node;
            VarRefNode *n = arena_alloc(ctx->arena, sizeof(VarRefNode));
            *n = *orig;
            if (orig->name) {
                n->name = arena_strdup(ctx->arena, orig->name);
                for (int i = 0; i < num_renames; i++) {
                    if (strcmp(orig->name, rename_from[i]) == 0) {
                        n->name = arena_strdup(ctx->arena, rename_to[i]);
                        printf("Cloned VarRef: %s -> %s\n", orig->name, n->name);
                        break;
                    }
                }
            }
            clone = (ASTNode*)n;
            break;
        }
        case NODE_FUNC_DEF: {
            FuncDefNode *orig = (FuncDefNode*)node;
            FuncDefNode *n = arena_alloc(ctx->arena, sizeof(FuncDefNode));
            *n = *orig;
            if (orig->name) {
                n->name = arena_strdup(ctx->arena, orig->name);
                for (int i = 0; i < num_renames; i++) {
                    if (strcmp(orig->name, rename_from[i]) == 0) {
                        n->name = arena_strdup(ctx->arena, rename_to[i]);
                        break;
                    }
                }
                if (strncmp(orig->name, "as_", 3) == 0) {
                    for (int i = 0; i < num_params; i++) {
                        if (strcmp(orig->name + 3, type_params[i]) == 0) {
                            char buf[256];
                            const char *repl = replace_with[i].base == TYPE_INT ? "int" :
                                               replace_with[i].base == TYPE_FLOAT ? "float" :
                                               replace_with[i].base == TYPE_DOUBLE ? "double" :
                                               replace_with[i].base == TYPE_BOOL ? "bool" :
                                               replace_with[i].class_name ? replace_with[i].class_name : "unknown";
                            snprintf(buf, sizeof(buf), "as_%s", repl);
                            n->name = arena_strdup(ctx->arena, buf);
                            break;
                        }
                    }
                }
            }
            if (orig->class_name) {
                n->class_name = arena_strdup(ctx->arena, orig->class_name);
                for (int i = 0; i < num_renames; i++) {
                    if (strcmp(orig->class_name, rename_from[i]) == 0) {
                        n->class_name = arena_strdup(ctx->arena, rename_to[i]);
                        break;
                    }
                }
            }
            n->ret_type = clone_var_type(ctx, orig->ret_type, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            
            Parameter *orig_p = orig->params;
            Parameter *new_p_head = NULL;
            Parameter **new_p_curr = &new_p_head;
            while(orig_p) {
                Parameter *np = arena_alloc(ctx->arena, sizeof(Parameter));
                *np = *orig_p;
                if (orig_p->name) np->name = arena_strdup(ctx->arena, orig_p->name);
                np->type = clone_var_type(ctx, orig_p->type, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
                np->next = NULL;
                *new_p_curr = np;
                new_p_curr = &np->next;
                orig_p = orig_p->next;
            }
            n->params = new_p_head;
            n->body = ast_clone(ctx, orig->body, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_VAR_DECL: {
            VarDeclNode *orig = (VarDeclNode*)node;
            VarDeclNode *n = arena_alloc(ctx->arena, sizeof(VarDeclNode));
            *n = *orig;
            if (orig->name) n->name = arena_strdup(ctx->arena, orig->name);
            n->var_type = clone_var_type(ctx, orig->var_type, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->initializer = ast_clone(ctx, orig->initializer, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->array_size = ast_clone(ctx, orig->array_size, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_CALL: {
            CallNode *orig = (CallNode*)node;
            CallNode *n = arena_alloc(ctx->arena, sizeof(CallNode));
            *n = *orig;
            if (orig->name) {
                char *bracket = strchr(orig->name, '[');
                if (bracket) {
                    char base_name[256];
                    int len = bracket - orig->name;
                    strncpy(base_name, orig->name, len);
                    base_name[len] = '\0';
                    
                    char mangled[1024];
                    strcpy(mangled, base_name);
                    for (int i = 0; i < num_renames; i++) {
                        if (strcmp(base_name, rename_from[i]) == 0) {
                            strcpy(mangled, rename_to[i]);
                            break;
                        }
                    }
                    n->name = arena_strdup(ctx->arena, mangled);
                } else {
                    n->name = arena_strdup(ctx->arena, orig->name);
                    for (int i = 0; i < num_renames; i++) {
                        if (strcmp(orig->name, rename_from[i]) == 0) {
                            n->name = arena_strdup(ctx->arena, rename_to[i]);
                            break;
                        }
                    }
                }
            }
            n->target = ast_clone(ctx, orig->target, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->args = ast_clone(ctx, orig->args, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_CLASS: {
            ClassNode *orig = (ClassNode*)node;
            ClassNode *n = arena_alloc(ctx->arena, sizeof(ClassNode));
            *n = *orig;
            if (orig->name) {
                n->name = arena_strdup(ctx->arena, orig->name);
                for (int i = 0; i < num_renames; i++) {
                    if (strcmp(orig->name, rename_from[i]) == 0) {
                        n->name = arena_strdup(ctx->arena, rename_to[i]);
                        break;
                    }
                }
            }
            if (orig->parent_name) n->parent_name = arena_strdup(ctx->arena, orig->parent_name);
            // Ignore cloning Traits for now, or just do it
            n->members = ast_clone(ctx, orig->members, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_BINARY_OP: {
            BinaryOpNode *orig = (BinaryOpNode*)node;
            BinaryOpNode *n = arena_alloc(ctx->arena, sizeof(BinaryOpNode));
            *n = *orig;
            n->left = ast_clone(ctx, orig->left, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->right = ast_clone(ctx, orig->right, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_UNARY_OP: {
            UnaryOpNode *orig = (UnaryOpNode*)node;
            UnaryOpNode *n = arena_alloc(ctx->arena, sizeof(UnaryOpNode));
            *n = *orig;
            n->operand = ast_clone(ctx, orig->operand, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }

        case NODE_RETURN: {
            ReturnNode *orig = (ReturnNode*)node;
            ReturnNode *n = arena_alloc(ctx->arena, sizeof(ReturnNode));
            *n = *orig;
            n->value = ast_clone(ctx, orig->value, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_CAST: {
            CastNode *orig = (CastNode*)node;
            CastNode *n = arena_alloc(ctx->arena, sizeof(CastNode));
            *n = *orig;
            n->var_type = clone_var_type(ctx, orig->var_type, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->operand = ast_clone(ctx, orig->operand, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_MEMBER_ACCESS: {
            MemberAccessNode *orig = (MemberAccessNode*)node;
            MemberAccessNode *n = arena_alloc(ctx->arena, sizeof(MemberAccessNode));
            *n = *orig;
            n->object = ast_clone(ctx, orig->object, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            if (orig->member_name) n->member_name = arena_strdup(ctx->arena, orig->member_name);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_METHOD_CALL: {
            MethodCallNode *orig = (MethodCallNode*)node;
            MethodCallNode *n = arena_alloc(ctx->arena, sizeof(MethodCallNode));
            *n = *orig;
            n->object = ast_clone(ctx, orig->object, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            if (orig->method_name) n->method_name = arena_strdup(ctx->arena, orig->method_name);
            n->args = ast_clone(ctx, orig->args, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            if (orig->mangled_name) n->mangled_name = arena_strdup(ctx->arena, orig->mangled_name);
            if (orig->owner_class) n->owner_class = arena_strdup(ctx->arena, orig->owner_class);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_INC_DEC: {
            IncDecNode *orig = (IncDecNode*)node;
            IncDecNode *n = arena_alloc(ctx->arena, sizeof(IncDecNode));
            *n = *orig;
            n->target = ast_clone(ctx, orig->target, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            if (orig->overloaded_func_name) n->overloaded_func_name = arena_strdup(ctx->arena, orig->overloaded_func_name);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_ARRAY_ACCESS: {
            ArrayAccessNode *orig = (ArrayAccessNode*)node;
            ArrayAccessNode *n = arena_alloc(ctx->arena, sizeof(ArrayAccessNode));
            *n = *orig;
            n->target = ast_clone(ctx, orig->target, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->index = ast_clone(ctx, orig->index, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_IF: {
            IfNode *orig = (IfNode*)node;
            IfNode *n = arena_alloc(ctx->arena, sizeof(IfNode));
            *n = *orig;
            n->condition = ast_clone(ctx, orig->condition, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->then_body = ast_clone(ctx, orig->then_body, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->else_body = ast_clone(ctx, orig->else_body, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_WHILE: {
            WhileNode *orig = (WhileNode*)node;
            WhileNode *n = arena_alloc(ctx->arena, sizeof(WhileNode));
            *n = *orig;
            n->condition = ast_clone(ctx, orig->condition, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->body = ast_clone(ctx, orig->body, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_ASSIGN: {
            AssignNode *orig = (AssignNode*)node;
            AssignNode *n = arena_alloc(ctx->arena, sizeof(AssignNode));
            *n = *orig;
            if (orig->name) n->name = arena_strdup(ctx->arena, orig->name);
            n->value = ast_clone(ctx, orig->value, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->target = ast_clone(ctx, orig->target, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->index = ast_clone(ctx, orig->index, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            if (orig->overloaded_func_name) n->overloaded_func_name = arena_strdup(ctx->arena, orig->overloaded_func_name);
            clone = (ASTNode*)n;
            break;
        }
        case NODE_SIZEOF:
        case NODE_ALIGNOF: {
            SizeOfNode *orig = (SizeOfNode*)node;
            SizeOfNode *n = arena_alloc(ctx->arena, sizeof(SizeOfNode));
            *n = *orig;
            n->target_type = clone_var_type(ctx, orig->target_type, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            if (orig->operand) {
                n->operand = ast_clone(ctx, orig->operand, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            }
            clone = (ASTNode*)n;
            break;
        }
        case NODE_CLEAN: {
            CleanNode *orig = (CleanNode*)node;
            CleanNode *n = arena_alloc(ctx->arena, sizeof(CleanNode));
            n->base.type = NODE_CLEAN;
            n->base.line = orig->base.line;
            n->base.col = orig->base.col;
            n->var_name = orig->var_name ? arena_strdup(ctx->arena, orig->var_name) : NULL;
            n->pristine_var_name = orig->pristine_var_name ? arena_strdup(ctx->arena, orig->pristine_var_name) : NULL;
            n->err_var_name = orig->err_var_name ? arena_strdup(ctx->arena, orig->err_var_name) : NULL;
            n->body = ast_clone(ctx, orig->body, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            n->residue_body = ast_clone(ctx, orig->residue_body, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            return (ASTNode*)n;
        }
        case NODE_UNTAINT: {
            UntaintNode *orig = (UntaintNode*)node;
            UntaintNode *n = arena_alloc(ctx->arena, sizeof(UntaintNode));
            n->base.type = NODE_UNTAINT;
            n->base.line = orig->base.line;
            n->base.col = orig->base.col;
            n->var_name = orig->var_name ? arena_strdup(ctx->arena, orig->var_name) : NULL;
            n->err_var_name = orig->err_var_name ? arena_strdup(ctx->arena, orig->err_var_name) : NULL;
            n->residue_body = ast_clone(ctx, orig->residue_body, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
            return (ASTNode*)n;
        }
        case NODE_ERRNUM:
            // Top level constructs are typically not cloned in template instantiation
            return NULL;
        default:
            clone = arena_alloc(ctx->arena, sizeof(ASTNode));
            *clone = *node;
            break;
    }
    
    if (node->next) {
        clone->next = ast_clone(ctx, node->next, type_params, replace_with, num_params, rename_from, rename_to, num_renames);
    } else {
        clone->next = NULL;
    }
    
    return clone;
}

ASTNode* ast_rewrite_macro(CompilerContext *ctx, ASTNode *node, ASTNode *varargs_head, char **param_names, ASTNode **param_args, int num_params) {
    if (!node) return NULL;
    
    // Check for parameter replacement
    if (node->type == NODE_VAR_REF) {
        VarRefNode *vn = (VarRefNode*)node;
        for (int i=0; i<num_params; i++) {
            if (param_names[i] && strcmp(vn->name, param_names[i]) == 0) {
                // Replace with a clone of the argument to avoid sharing nodes
                return ast_clone(ctx, param_args[i], NULL, NULL, 0, NULL, NULL, 0);
            }
        }
    }
    
    // Check for ...[N]
    if (node->type == NODE_ARRAY_ACCESS) {
        ArrayAccessNode *aa = (ArrayAccessNode*)node;
        if (aa->target && aa->target->type == NODE_VAR_REF) {
            VarRefNode *vn = (VarRefNode*)aa->target;
            if (strcmp(vn->name, "...") == 0) {
                if (aa->index && aa->index->type == NODE_LITERAL) {
                    LiteralNode *ln = (LiteralNode*)aa->index;
                    int idx = (int)ln->val.long_val;
                    
                    // Traverse varargs to find the N-th argument
                    ASTNode *curr = varargs_head;
                    for (int i=0; i<idx && curr; i++) {
                        curr = curr->next;
                    }
                    if (curr) {
                        return ast_clone(ctx, curr, NULL, NULL, 0, NULL, NULL, 0);
                    }
                }
            }
        }
    }
    
    // Recurse down the tree
    // We modify the cloned AST in place!
    switch (node->type) {
        case NODE_FUNC_DEF: {
            FuncDefNode *fn = (FuncDefNode*)node;
            fn->body = ast_rewrite_macro(ctx, fn->body, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_VAR_DECL: {
            VarDeclNode *vn = (VarDeclNode*)node;
            vn->initializer = ast_rewrite_macro(ctx, vn->initializer, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_ASSIGN: {
            AssignNode *an = (AssignNode*)node;
            an->target = ast_rewrite_macro(ctx, an->target, varargs_head, param_names, param_args, num_params);
            an->value = ast_rewrite_macro(ctx, an->value, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_BINARY_OP: {
            BinaryOpNode *bn = (BinaryOpNode*)node;
            bn->left = ast_rewrite_macro(ctx, bn->left, varargs_head, param_names, param_args, num_params);
            bn->right = ast_rewrite_macro(ctx, bn->right, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_CALL: {
            CallNode *cn = (CallNode*)node;
            cn->target = ast_rewrite_macro(ctx, cn->target, varargs_head, param_names, param_args, num_params);
            cn->args = ast_rewrite_macro(ctx, cn->args, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_METHOD_CALL: {
            MethodCallNode *mcn = (MethodCallNode*)node;
            mcn->object = ast_rewrite_macro(ctx, mcn->object, varargs_head, param_names, param_args, num_params);
            mcn->args = ast_rewrite_macro(ctx, mcn->args, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_RETURN: {
            ReturnNode *rn = (ReturnNode*)node;
            rn->value = ast_rewrite_macro(ctx, rn->value, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_IF: {
            IfNode *in = (IfNode*)node;
            in->condition = ast_rewrite_macro(ctx, in->condition, varargs_head, param_names, param_args, num_params);
            in->then_body = ast_rewrite_macro(ctx, in->then_body, varargs_head, param_names, param_args, num_params);
            in->else_body = ast_rewrite_macro(ctx, in->else_body, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_ARRAY_ACCESS: {
            ArrayAccessNode *an = (ArrayAccessNode*)node;
            an->target = ast_rewrite_macro(ctx, an->target, varargs_head, param_names, param_args, num_params);
            an->index = ast_rewrite_macro(ctx, an->index, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_UNARY_OP: {
            UnaryOpNode *un = (UnaryOpNode*)node;
            un->operand = ast_rewrite_macro(ctx, un->operand, varargs_head, param_names, param_args, num_params);
            break;
        }
        case NODE_TYPEOF:
        case NODE_SIZEOF: {
            UnaryOpNode *un = (UnaryOpNode*)node;
            un->operand = ast_rewrite_macro(ctx, un->operand, varargs_head, param_names, param_args, num_params);
            break;
        }
        default:
            break;
    }
    
    // Also rewrite next nodes in the list!
    node->next = ast_rewrite_macro(ctx, node->next, varargs_head, param_names, param_args, num_params);
    
    return node;
}
