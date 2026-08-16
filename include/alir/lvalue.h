/**
 * @file lvalue.h
 * @brief ALIR lvalue and expression generation.
 */
#ifndef ALIR_LVALUE_H
#define ALIR_LVALUE_H

#include "alir.h"

/**
 * @brief Generates the address of an AST node.
 * @param ctx The ALIR context.
 * @param node The AST node.
 * @return The address as an ALIR value.
 */
AlirValue* alir_gen_addr(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Generates ALIR for an array literal.
 * @param ctx The ALIR context.
 * @param node The array literal AST node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_array_lit(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Generates ALIR for a literal node.
 * @param ctx The ALIR context.
 * @param ln The literal node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_literal(AlirCtx *ctx, LiteralNode *ln);

/**
 * @brief Generates ALIR for a variable reference.
 * @param ctx The ALIR context.
 * @param vn The variable reference node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_var_ref(AlirCtx *ctx, VarRefNode *vn);

/**
 * @brief Generates ALIR for a member access.
 * @param ctx The ALIR context.
 * @param node The access AST node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_access(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Generates ALIR for a binary operation.
 * @param ctx The ALIR context.
 * @param bn The binary operation node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_binary_op(AlirCtx *ctx, BinaryOpNode *bn);

/**
 * @brief Generates ALIR for a unary operation.
 * @param ctx The ALIR context.
 * @param un The unary operation node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_unary_op(AlirCtx *ctx, UnaryOpNode *un);

/**
 * @brief Generates ALIR for an increment/decrement operation.
 * @param ctx The ALIR context.
 * @param id The increment/decrement node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_inc_dec(AlirCtx *ctx, IncDecNode *id);

/**
 * @brief Generates ALIR for a cast.
 * @param ctx The ALIR context.
 * @param cn The cast node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_cast(AlirCtx *ctx, CastNode *cn);

/**
 * @brief Generates ALIR for a standard function call.
 * @param ctx The ALIR context.
 * @param cn The call node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_call_std(AlirCtx *ctx, CallNode *cn);

/**
 * @brief Generates ALIR for a function call.
 * @param ctx The ALIR context.
 * @param cn The call node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_call(AlirCtx *ctx, CallNode *cn);

/**
 * @brief Generates ALIR for a method call.
 * @param ctx The ALIR context.
 * @param mc The method call node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_method_call(AlirCtx *ctx, MethodCallNode *mc);

/**
 * @brief Generates ALIR for a generic expression.
 * @param ctx The ALIR context.
 * @param node The expression AST node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_expr(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Generates ALIR for an index access expression.
 * @param ctx The ALIR context.
 * @param aa The index access node.
 * @return The resulting ALIR value.
 */
AlirValue* alir_gen_expr_index_access(AlirCtx *ctx, IndexAccessNode *aa);

/**
 * @brief Generates ALIR for the address of an index access.
 * @param ctx The ALIR context.
 * @param aa The index access node.
 * @return The resulting address ALIR value.
 */
AlirValue* alir_gen_addr_index_access(AlirCtx *ctx, IndexAccessNode *aa);

/**
 * @brief Returns the size in bytes of a type.
 * @param t The type.
 * @return The size in bytes.
 */
int alir_get_type_size(VarType t);

/**
 * @brief Returns the size in bytes of a struct type.
 * @param mod The ALIR module.
 * @param struct_name The struct name.
 * @return The size in bytes, or -1 on failure.
 */
int alir_get_struct_size(AlirModule *mod, const char *struct_name);

/**
 * @brief Robustly finds a field index, falling back to heuristics.
 * @param ctx The ALIR context.
 * @param hint_class A hint for the class name.
 * @param field_name The field name.
 * @return The field index, or -1 on failure.
 */
int alir_robust_get_field_index(AlirCtx *ctx, const char *hint_class, const char *field_name);

#endif // ALIR_LVALUE_H
