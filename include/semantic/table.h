/**
 * @file table.h
 * @brief Semantic hash table utilities.
 */
#ifndef SEMANTIC_TABLE_H
#define SEMANTIC_TABLE_H

/**
 * @brief Hashes an AST node pointer.
 * @param node The AST node.
 * @return The hash value.
 */
unsigned int hash_ptr(ASTNode *node);

#endif // SEMANTIC_TABLE_H
