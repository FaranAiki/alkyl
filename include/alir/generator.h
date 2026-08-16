/**
 * @file generator.h
 * @brief ALIR generation routines.
 */
#ifndef ALIR_GENERATOR_H
#define ALIR_GENERATOR_H

#include "alir.h"

/**
 * @brief Pushes a new loop context onto the loop stack.
 * @param ctx The ALIR context.
 * @param cont The continue target block.
 * @param brk The break target block.
 */
void push_loop(AlirCtx *ctx, AlirBlock *cont, AlirBlock *brk);

/**
 * @brief Pops the current loop context from the loop stack.
 * @param ctx The ALIR context.
 */
void pop_loop(AlirCtx *ctx);

/**
 * @brief Checks if an opcode is a terminator instruction.
 * @param op The opcode to check.
 * @return Non-zero if the opcode terminates a block.
 */
int is_terminator(AlirOpcode op); 

/**
 * @brief Evaluates an AST node as a constant integer.
 * @param ctx The ALIR context.
 * @param node The AST node to evaluate.
 * @return The evaluated integer value.
 */
long alir_eval_constant_int(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Attempts to fold an AST expression into a constant value.
 * @param ctx The ALIR context.
 * @param node The AST expression node.
 * @param target The desired target type.
 * @return The folded constant value, or NULL if not constant.
 */
AlirValue* alir_fold_const_expr(AlirCtx *ctx, ASTNode *node, VarType target);

/**
 * @brief Scans an AST and folds constant expressions.
 * @param ctx The ALIR context.
 * @param root The root AST node.
 */
void scan_and_fold_consts(AlirCtx *ctx, ASTNode *root);

/**
 * @brief Builds struct field descriptors from a class node.
 * @param ctx The ALIR context.
 * @param cn The class node.
 * @param st The ALIR struct to populate.
 */
void build_struct_fields(AlirCtx *ctx, ClassNode *cn, AlirStruct *st);

/**
 * @brief First pass: registers all symbols in the current namespace.
 * @param ctx The ALIR context.
 * @param n The AST node to process.
 * @param current_ns The current namespace name.
 */
void pass1_register(AlirCtx *ctx, ASTNode *n, const char *current_ns);

/**
 * @brief Second pass: populates function bodies and resolves references.
 * @param ctx The ALIR context.
 * @param root The root AST node.
 * @param n The AST node to process.
 * @param current_ns The current namespace name.
 */
void pass2_populate(AlirCtx *ctx, ASTNode *root, ASTNode *n, const char *current_ns);

/**
 * @brief Scans the AST and registers all classes.
 * @param ctx The ALIR context.
 * @param root The root AST node.
 */
void alir_scan_and_register_classes(AlirCtx *ctx, ASTNode *root);

/**
 * @brief Generates ALIR for a switch statement.
 * @param ctx The ALIR context.
 * @param sn The switch AST node.
 */
void alir_gen_switch(AlirCtx *ctx, SwitchNode *sn);

/**
 * @brief Generates an implicit constructor for a class.
 * @param ctx The ALIR context.
 * @param cn The class node.
 * @param fqn The fully qualified class name.
 */
void alir_gen_implicit_constructor(AlirCtx *ctx, ClassNode *cn, const char *fqn);

/**
 * @brief Generates inherited methods for a class.
 * @param ctx The ALIR context.
 * @param cn The class node.
 * @param class_name The class name.
 * @param target_node The target class node for inheritance.
 */
void alir_gen_inherited_methods(AlirCtx *ctx, ClassNode *cn, const char *class_name, ClassNode *target_node);

/**
 * @brief Recursively generates ALIR for all functions in the AST.
 * @param ctx The ALIR context.
 * @param root The root AST node.
 * @param current_ns The current namespace name.
 */
void alir_gen_functions_recursive(AlirCtx *ctx, ASTNode *root, const char *current_ns);

/**
 * @brief Generates ALIR for a single function definition.
 * @param ctx The ALIR context.
 * @param fn The function definition node.
 * @param class_name The class name, or NULL for free functions.
 */
void alir_gen_function_def(AlirCtx *ctx, FuncDefNode *fn, const char *class_name);

/**
 * @brief Generates the complete ALIR module from a semantic context.
 * @param sem The semantic context.
 * @param root The root AST node.
 * @return The generated ALIR module.
 */
AlirModule* alir_generate(SemanticCtx *sem, ASTNode *root);

#endif // ALIR_GENERATOR_H
