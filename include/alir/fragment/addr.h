/**
 * @file addr.h
 * @brief ALIR address generation helpers.
 */
#ifndef ALIR_FRAGMENT_ADDR_H
#define ALIR_FRAGMENT_ADDR_H

/**
 * @brief Generates the address of a variable reference.
 * @param ctx The ALIR context.
 * @param node The AST node.
 * @return The address as an ALIR value.
 */
AlirValue* alir_gen_addr_var_ref(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Generates the address of a member access.
 * @param ctx The ALIR context.
 * @param node The AST node.
 * @return The address as an ALIR value.
 */
AlirValue* alir_gen_addr_member_access(AlirCtx *ctx, ASTNode *node);

#endif // ALIR_FRAGMENT_ADDR_H
