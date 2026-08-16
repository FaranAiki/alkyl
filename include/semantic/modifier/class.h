/**
 * @file class.h
 * @brief Class-related semantic checking declarations.
 */
#ifndef SEMANTIC_MODIFIER_CLASS_H
#define SEMANTIC_MODIFIER_CLASS_H

#include "semantic.h"

/**
 * @brief Checks a member access expression.
 * @param ctx The semantic context.
 * @param node The member access node.
 */
void sem_check_member_access(SemanticCtx *ctx, MemberAccessNode *node);

/**
 * @brief Scans class members and registers them.
 * @param ctx The semantic context.
 * @param cn The class node.
 * @param class_sym The class symbol.
 */
void sem_scan_class_members(SemanticCtx *ctx, ClassNode *cn, SemSymbol *class_sym);

#endif // SEMANTIC_MODIFIER_CLASS_H

