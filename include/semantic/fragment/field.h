/**
 * @file field.h
 * @brief Class field counting and collection declarations.
 */
#ifndef SEMANTIC_FIELD_H
#define SEMANTIC_FIELD_H

/**
 * @brief Counts the total number of fields in a class.
 * @param ctx The semantic context.
 * @param sym The class symbol.
 * @return The number of fields.
 */
int sem_count_class_fields(SemanticCtx *ctx, SemSymbol *sym);

/**
 * @brief Counts the number of required fields in a class.
 * @param ctx The semantic context.
 * @param sym The class symbol.
 * @return The number of required fields.
 */
int sem_count_required_class_fields(SemanticCtx *ctx, SemSymbol *sym);

/**
 * @brief Collects class fields into an array.
 * @param ctx The semantic context.
 * @param sym The class symbol.
 * @param fields Output array of field nodes.
 * @param idx Pointer to the current index.
 */
void sem_collect_class_fields(SemanticCtx *ctx, SemSymbol *sym, VarDeclNode **fields, int *idx);

#endif // SEMANTIC_FIELD_H
