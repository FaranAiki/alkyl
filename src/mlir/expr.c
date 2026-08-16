/**
 * @file expr.c
 * @brief MLIR expression generation implementation.
 */
#include "mlir/generate.h"
#include "common/hashmap.h"
#include <stdio.h>

AlkylMlirValue mlir_gen_expr(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node) {
    if (!node) return NULL;

    switch (node->type) {
        case NODE_LITERAL: {
            LiteralNode *lit = (LiteralNode*)node;
            if (lit->var_type.base == TYPE_CHAR && lit->var_type.ptr_depth > 0) {
                return alkyl_mlir_build_string_constant(ctx, mod, lit->val.str_val);
            }
            return alkyl_mlir_build_int_constant(ctx, lit->val.int_val);
        }
        case NODE_VAR_REF: {
            VarRefNode *var = (VarRefNode*)node;
            extern HashMap *mlir_vars;
            if (mlir_vars && var->name) {
                AlkylMlirValue alloca = hashmap_get(mlir_vars, var->name);
                if (alloca) {
                    return alkyl_mlir_build_load(ctx, alloca);
                }
            }

            // If not found in mlir_vars, check if it's an enum variant
            if (var->name) {
                extern ASTNode *mlir_global_ast_root;
                ASTNode *n = mlir_global_ast_root;
                while (n) {
                    if (n->type == NODE_ENUM) {
                        EnumNode *en = (EnumNode*)n;
                        EnumEntry *ent = en->entries;
                        while (ent) {
                            if (strcmp(ent->name, var->name) == 0) {
                                return alkyl_mlir_build_int_constant(ctx, ent->value);
                            }
                            ent = ent->next;
                        }
                    }
                    n = n->next;
                }
            }
            return alkyl_mlir_build_int_constant(ctx, 0);
        }
        case NODE_CALL: {
            CallNode *call = (CallNode*)node;

            // Handle "print" macro if we are bypassing ALIR
            if (call->name && strcmp(call->name, "print") == 0) {
                ASTNode *arg = call->args;
                while (arg) {
                    AlkylMlirValue arg_val = mlir_gen_expr(ctx, mod, arg);

                    // We generate a call to cprintf for each arg.
                    // For a proper implementation, we'd use "%d" or "%s" based on type.
                    // But for this MVP, we'll just pass it to cprintf directly if it's a string,
                    // or use a generic print function. Let's just pass it to cprintf as-is for strings,
                    // or we might need format strings. Actually, to make it exactly match without hardcoding,
                    // we'd need to emit "%d" for integers.
                    if (arg->sem_type.base == TYPE_INT || arg->sem_type.base == TYPE_UNSIGNED_INT) {
                        AlkylMlirValue fmt = alkyl_mlir_build_string_constant(ctx, mod, "%d");
                        AlkylMlirValue cprintf_args[2] = {fmt, arg_val};
                        alkyl_mlir_build_call(ctx, "printf", cprintf_args, 2);
                    } else if (arg->sem_type.base == TYPE_CHAR && arg->sem_type.ptr_depth > 0) {
                        AlkylMlirValue cprintf_args[1] = {arg_val};
                        alkyl_mlir_build_call(ctx, "printf", cprintf_args, 1);
                    } else {
                        // Fallback generic
                        AlkylMlirValue fmt = alkyl_mlir_build_string_constant(ctx, mod, "%d");
                        AlkylMlirValue cprintf_args[2] = {fmt, arg_val};
                        alkyl_mlir_build_call(ctx, "printf", cprintf_args, 2);
                    }

                    arg = arg->next;
                }
                return alkyl_mlir_build_int_constant(ctx, 0);
            }

            // Check if it's a constructor (Class type)
            if (call->base.sem_type.base == TYPE_CLASS) {
                // Find class to count fields
                extern ASTNode *mlir_global_ast_root;
                ASTNode *n = mlir_global_ast_root;
                int num_fields = 0;
                while (n) {
                    if (n->type == NODE_CLASS) {
                        ClassNode *cls = (ClassNode*)n;
                        if (strcmp(cls->name, call->base.sem_type.class_name) == 0) {
                            ASTNode *member = cls->members;
                            while (member) {
                                if (member->type == NODE_VAR_DECL) num_fields++;
                                member = member->next;
                            }
                            break;
                        }
                    }
                    n = n->next;
                }
                int arg_count = 0;
                ASTNode *a = call->args;
                while (a) { arg_count++; a = a->next; }
                if (arg_count > num_fields) num_fields = arg_count;

                AlkylMlirValue obj = alkyl_mlir_build_alloc_object(ctx, num_fields);

                // Evaluate arguments and store in fields
                int i = 0;
                ASTNode *arg = call->args;
                while (arg && i < num_fields) {
                    AlkylMlirValue arg_val = mlir_gen_expr(ctx, mod, arg);
                    alkyl_mlir_build_store_field(ctx, arg_val, obj, i);
                    i++;
                    arg = arg->next;
                }
                return obj;
            }

            // Evaluate arguments
            AlkylMlirValue args[32];
            int num_args = 0;
            ASTNode *arg = call->args;
            while (arg && num_args < 32) {
                args[num_args++] = mlir_gen_expr(ctx, mod, arg);
                arg = arg->next;
            }
            return alkyl_mlir_build_call(ctx, call->mangled_name ? call->mangled_name : (call->name ? call->name : "unknown"), args, num_args);
        }
        case NODE_BINARY_OP: {
            BinaryOpNode *binop = (BinaryOpNode*)node;
            AlkylMlirValue lhs = mlir_gen_expr(ctx, mod, binop->left);
            AlkylMlirValue rhs = mlir_gen_expr(ctx, mod, binop->right);

            if (binop->op == TOKEN_PLUS) return alkyl_mlir_build_add(ctx, lhs, rhs);
            if (binop->op == TOKEN_MINUS) return alkyl_mlir_build_sub(ctx, lhs, rhs);
            if (binop->op == TOKEN_STAR) return alkyl_mlir_build_mul(ctx, lhs, rhs);
            if (binop->op == TOKEN_SLASH) return alkyl_mlir_build_div(ctx, lhs, rhs);
            if (binop->op == TOKEN_MOD) return alkyl_mlir_build_mod(ctx, lhs, rhs);
            if (binop->op == TOKEN_LSHIFT) return alkyl_mlir_build_shl(ctx, lhs, rhs);
            if (binop->op == TOKEN_RSHIFT) return alkyl_mlir_build_shr(ctx, lhs, rhs);
            if (binop->op == TOKEN_AND) return alkyl_mlir_build_and(ctx, lhs, rhs);
            if (binop->op == TOKEN_OR) return alkyl_mlir_build_or(ctx, lhs, rhs);
            if (binop->op == TOKEN_XOR) return alkyl_mlir_build_xor(ctx, lhs, rhs);
            break;
        }
        case NODE_METHOD_CALL: {
            MethodCallNode *mcall = (MethodCallNode*)node;
            AlkylMlirValue args[32];
            int num_args = 0;

            if (!mcall->is_static) {
                args[num_args++] = mlir_gen_expr(ctx, mod, mcall->object);
            }

            ASTNode *arg = mcall->args;
            while (arg && num_args < 32) {
                args[num_args++] = mlir_gen_expr(ctx, mod, arg);
                arg = arg->next;
            }

            char func_name_buf[512];
            char *func_name = mcall->mangled_name ? mcall->mangled_name : mcall->method_name;

            if (mcall->object && mcall->object->type == NODE_INDEX_ACCESS) {
                IndexAccessNode *ia = (IndexAccessNode*)mcall->object;
                if (ia->target) {
                    VarType obj_t = ia->target->sem_type;
                    if (obj_t.base == TYPE_CLASS && obj_t.class_name) {
                        snprintf(func_name_buf, sizeof(func_name_buf), "main_%s_%s", obj_t.class_name, mcall->method_name);
                        func_name = func_name_buf;
                    }
                }
            } else if (mcall->object && mcall->object->type == NODE_MEMBER_ACCESS) {
                MemberAccessNode *ma = (MemberAccessNode*)mcall->object;
                if (ma->object && ma->object->type == NODE_INDEX_ACCESS) {
                    IndexAccessNode *ia = (IndexAccessNode*)ma->object;
                    if (ia->target) {
                        VarType obj_t = ia->target->sem_type;
                        if (obj_t.base == TYPE_CLASS && obj_t.class_name) {
                            snprintf(func_name_buf, sizeof(func_name_buf), "main_%s_%s", obj_t.class_name, mcall->method_name);
                            func_name = func_name_buf;
                        }
                    }
                }
            }
            return alkyl_mlir_build_call(ctx, func_name, args, num_args);
        }
        case NODE_MEMBER_ACCESS: {
            MemberAccessNode *maccess = (MemberAccessNode*)node;
            debug_mlir("member access %s, base=%d, expected TYPE_ENUM=%d, class_name=%s\n", maccess->member_name, maccess->object->sem_type.base, TYPE_ENUM, maccess->object->sem_type.class_name);
            if (maccess->object->sem_type.base == TYPE_ENUM && maccess->object->sem_type.class_name) {
                extern ASTNode *mlir_global_ast_root;
                ASTNode *n = mlir_global_ast_root;
                while (n) {
                    if (n->type == NODE_ENUM) {
                        EnumNode *en = (EnumNode*)n;
                        if (strcmp(en->name, maccess->object->sem_type.class_name) == 0) {
                            EnumEntry *ent = en->entries;
                            long val = 0;
                            while (ent) {
                                if (strcmp(ent->name, maccess->member_name) == 0) {
                                    val = ent->value;
                                    break;
                                }
                                ent = ent->next;
                            }
                            return alkyl_mlir_build_int_constant(ctx, val);
                        }
                    }
                    n = n->next;
                }
                return alkyl_mlir_build_int_constant(ctx, 0);
            }
            AlkylMlirValue obj = mlir_gen_expr(ctx, mod, maccess->object);

            int index = 0;
            extern ASTNode *mlir_global_ast_root;
            ASTNode *n = mlir_global_ast_root;
            if (maccess->object->sem_type.class_name) {
                while (n) {
                    if (n->type == NODE_CLASS) {
                        ClassNode *cls = (ClassNode*)n;
                        if (strcmp(cls->name, maccess->object->sem_type.class_name) == 0) {
                            ASTNode *member = cls->members;
                            int idx = 0;
                            while (member) {
                                if (member->type == NODE_VAR_DECL) {
                                    VarDeclNode *vd = (VarDeclNode*)member;
                                    if (vd->name && maccess->member_name && strcmp(vd->name, maccess->member_name) == 0) {
                                        index = idx;
                                        break;
                                    }
                                    idx++;
                                }
                                member = member->next;
                            }
                            break;
                        }
                    }
                    n = n->next;
                }
            }
            if (index == 0) {
                // Fallback global search like ALIR does for inherited fields/traits
                n = mlir_global_ast_root;
                while (n) {
                    if (n->type == NODE_CLASS) {
                        ClassNode *cls = (ClassNode*)n;
                        ASTNode *member = cls->members;
                        int idx = 0;
                        int found = 0;
                        while (member) {
                            if (member->type == NODE_VAR_DECL) {
                                VarDeclNode *vd = (VarDeclNode*)member;
                                if (vd->name && maccess->member_name && strcmp(vd->name, maccess->member_name) == 0) {
                                    index = idx;
                                    found = 1;
                                    break;
                                }
                                idx++;
                            }
                            member = member->next;
                        }
                        if (found) break;
                    }
                    n = n->next;
                }
            }

            printf("Member access %s on %s: index %d\n", maccess->member_name, maccess->object->sem_type.class_name ? maccess->object->sem_type.class_name : "unknown", index);

            int is_string = (maccess->base.sem_type.base == TYPE_CHAR && maccess->base.sem_type.ptr_depth > 0);
            return alkyl_mlir_build_load_field(ctx, obj, index, is_string);
        }
        case NODE_INDEX_ACCESS: {
            IndexAccessNode *ia = (IndexAccessNode*)node;
            if (ia->target->type == NODE_VAR_REF) {
                char *enum_name = ((VarRefNode*)ia->target)->name;
                AlkylMlirValue index_val = mlir_gen_expr(ctx, mod, ia->index);
                AlkylMlirValue dest = alkyl_mlir_build_alloc_object(ctx, 1);

                extern ASTNode *mlir_global_ast_root;
                ASTNode *n = mlir_global_ast_root;
                while (n) {
                    if (n->type == NODE_ENUM) {
                        EnumNode *en = (EnumNode*)n;
                        if (strcmp(en->name, enum_name) == 0) {
                            int num_entries = 0;
                            EnumEntry *ent = en->entries;
                            while (ent) { num_entries++; ent = ent->next; }

                            void *switch_op = alkyl_mlir_build_switch_start(ctx, index_val, num_entries);

                            ent = en->entries;
                            while (ent) {
                                void alkyl_mlir_build_switch_set_cond_insertion(AlkylMlirContext c_ctx, void* switch_op_ptr);
                                alkyl_mlir_build_switch_set_cond_insertion(ctx, switch_op);
                                AlkylMlirValue case_val = alkyl_mlir_build_int_constant(ctx, ent->value);
                                alkyl_mlir_build_switch_case_start(ctx, switch_op, case_val, 0);
                                AlkylMlirValue str_val = alkyl_mlir_build_string_constant(ctx, mod, ent->name);
                                alkyl_mlir_build_store_field(ctx, str_val, dest, 0);
                                alkyl_mlir_build_switch_case_end(ctx, switch_op, 0);
                                ent = ent->next;
                            }

                            alkyl_mlir_build_switch_default_start(ctx, switch_op);
                            AlkylMlirValue unk_val = alkyl_mlir_build_string_constant(ctx, mod, "unknown");
                            alkyl_mlir_build_store_field(ctx, unk_val, dest, 0);
                            alkyl_mlir_build_switch_default_end(ctx, switch_op);

                            alkyl_mlir_build_switch_end(ctx, switch_op);
                            return alkyl_mlir_build_load_field(ctx, dest, 0, 1);
                        }
                    }
                    n = n->next;
                }
            }
            // Fallback for trait cast or array access: just return the object pointer
            return mlir_gen_expr(ctx, mod, ia->target);
        }
        case NODE_ASSIGN: {
            AssignNode *assign = (AssignNode*)node;
            AlkylMlirValue val = mlir_gen_expr(ctx, mod, assign->value);
            extern HashMap *mlir_vars;
            if (mlir_vars && assign->name) {
                AlkylMlirValue alloca = hashmap_get(mlir_vars, assign->name);
                if (alloca) {
                    alkyl_mlir_build_store(ctx, val, alloca);
                    return val;
                }
            }
            return alkyl_mlir_build_int_constant(ctx, 0);
        }
        case NODE_INC_DEC: {
            IncDecNode *inc = (IncDecNode*)node;
            extern HashMap *mlir_vars;
            char *var_name = inc->name;
            if (!var_name && inc->target && inc->target->type == NODE_VAR_REF) {
                var_name = ((VarRefNode*)inc->target)->name;
            }
            if (mlir_vars && var_name) {
                AlkylMlirValue alloca = hashmap_get(mlir_vars, var_name);
                if (alloca) {
                    AlkylMlirValue old_val = alkyl_mlir_build_load(ctx, alloca);
                    AlkylMlirValue one = alkyl_mlir_build_int_constant(ctx, 1);
                    AlkylMlirValue new_val = (inc->op == TOKEN_INCREMENT) ? alkyl_mlir_build_add(ctx, old_val, one) : alkyl_mlir_build_sub(ctx, old_val, one);
                    alkyl_mlir_build_store(ctx, new_val, alloca);
                    return inc->is_prefix ? new_val : old_val;
                }
            }
            return alkyl_mlir_build_int_constant(ctx, 0);
        }
        case NODE_UNARY_OP: {
            UnaryOpNode *unop = (UnaryOpNode*)node;
            AlkylMlirValue operand = mlir_gen_expr(ctx, mod, unop->operand);
            if (unop->op == TOKEN_MINUS) {
                AlkylMlirValue zero = alkyl_mlir_build_int_constant(ctx, 0);
                return alkyl_mlir_build_sub(ctx, zero, operand);
            } else if (unop->op == TOKEN_NOT) {
                AlkylMlirValue one = alkyl_mlir_build_int_constant(ctx, 1);
                return alkyl_mlir_build_xor(ctx, operand, one);
            } else if (unop->op == TOKEN_BIT_NOT) {
                AlkylMlirValue minus_one = alkyl_mlir_build_int_constant(ctx, -1);
                return alkyl_mlir_build_xor(ctx, operand, minus_one);
            } else if (unop->op == TOKEN_STAR) {
                return alkyl_mlir_build_load(ctx, operand);
            }
            return operand;
        }
        case NODE_ARRAY_LIT: {
            ArrayLitNode *arr = (ArrayLitNode*)node;
            int num_elements = 0;
            ASTNode *elem = arr->elements;
            while (elem) { num_elements++; elem = elem->next; }

            AlkylMlirValue obj = alkyl_mlir_build_alloc_object(ctx, num_elements);

            int i = 0;
            elem = arr->elements;
            while (elem && i < num_elements) {
                AlkylMlirValue val = mlir_gen_expr(ctx, mod, elem);
                alkyl_mlir_build_store_field(ctx, val, obj, i);
                i++;
                elem = elem->next;
            }
            return obj;
        }
        case NODE_CAST: {
            CastNode *cast = (CastNode*)node;
            return mlir_gen_expr(ctx, mod, cast->operand);
        }
        default:
            printf("Warning: Unhandled expr node type %d in MLIR\n", node->type);
            return alkyl_mlir_build_int_constant(ctx, 0);
    }
    return alkyl_mlir_build_int_constant(ctx, 0);
}
