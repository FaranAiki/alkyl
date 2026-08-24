/**
 * @file emitter.h
 * @brief Semantic symbol table emitter.
 */
#ifndef SEMANTIC_EMITTER_H
#define SEMANTIC_EMITTER_H

#include "semantic.h"
#include "../common/common.h"

/**
 * @brief Append indentation spaces to a string builder.
 * @param sb String builder to append to.
 * @param indent Number of indentation levels (2 spaces per level).
 */
void semantic_emit_indent(StringBuilder *sb, int indent);

/**
 * @brief Append a human-readable type string to a string builder.
 * @param sb String builder to append to.
 * @param t Type to render.
 */
void semantic_emit_type_str(StringBuilder *sb, VarType t);

/**
 * @brief Emit a single symbol entry (kind, name, type) to a string builder.
 * @param sb String builder to append to.
 * @param sym Symbol to emit.
 * @param indent Current indentation level.
 */
void semantic_emit_symbol(StringBuilder *sb, SemSymbol *sym, int indent);

/**
 * @brief Emit all symbols in a scope to a string builder.
 * @param sb String builder to append to.
 * @param scope Scope to emit.
 * @param indent Current indentation level.
 */
void semantic_emit_scope(StringBuilder *sb, SemScope *scope, int indent);

/**
 * @brief Serialize the entire semantic symbol table to a string.
 * @param ctx Semantic context.
 * @return Heap-allocated string representation of the symbol table (arena-backed via sb.data).
 */
char* semantic_to_string(SemanticCtx *ctx);

/**
 * @brief Write the semantic symbol table to a file.
 * @param ctx Semantic context.
 * @param filename Path of the output file.
 */
void semantic_to_file(SemanticCtx *ctx, const char *filename);

#endif // SEMANTIC_EMITTER_H
