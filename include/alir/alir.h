/**
 * @file alir.h
 * @brief Main ALIR (Alkyl Intermediate Representation) definitions.
 */
#ifndef ALIR_H
#define ALIR_H

#include "../common/hashmap.h"
#include "../semantic/semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    ALIR_VAL_VOID,
    ALIR_VAL_INT,
    ALIR_VAL_SINGLE,
    ALIR_VAL_DOUBLE,
    ALIR_VAL_VAR,       // Represents a global/function name (@name)
    ALIR_VAL_TEMP,      // Represents a temporary register (%0, %1)
    ALIR_VAL_LABEL,     // Represents a block label
    ALIR_VAL_CONST,     // Raw constant value
    ALIR_VAL_TYPE,      // Represents a type (for sizeof)
    ALIR_VAL_GLOBAL     // Represents a global variable/constant pointer
} AlirValueKind;

/**
 * @brief A value in ALIR.
 */
typedef struct AlirValue {
    AlirValueKind kind;
    VarType type;       // Reuse Parser's VarType for type info
    int temp_id;

    Value val;
} AlirValue;

/**
 * @brief ALIR opcodes.
 */
typedef enum {
    ALIR_OP_ALLOCA,
    ALIR_OP_FREE_STACK,
    ALIR_OP_STORE,
    ALIR_OP_LOAD,
    ALIR_OP_GET_PTR,    // Generic GEP (Get Element Ptr) for Arrays/Structs
    ALIR_OP_BITCAST,    // Replaces raw casting logic

    // Arithmetic
    ALIR_OP_ADD, ALIR_OP_SUB, ALIR_OP_MUL, ALIR_OP_DIV, ALIR_OP_MOD,
    ALIR_OP_FADD, ALIR_OP_FSUB, ALIR_OP_FMUL, ALIR_OP_FDIV,

    // Logical / Bitwise
    ALIR_OP_AND, ALIR_OP_OR, ALIR_OP_XOR, ALIR_OP_NOT,
    ALIR_OP_SHL, ALIR_OP_SHR, ALIR_OP_ROTR, ALIR_OP_ROTL,

    // Comparison
    ALIR_OP_LT, ALIR_OP_GT, ALIR_OP_LTE, ALIR_OP_GTE, ALIR_OP_EQ, ALIR_OP_NEQ,

    // Control Flow
    ALIR_OP_JUMP,         // Unconditional Branch
    ALIR_OP_CONDI,    // Conditional Branch
    ALIR_OP_CALL,
    ALIR_OP_RET,
    ALIR_OP_PANIC,
    ALIR_OP_UNREACHABLE,
    ALIR_OP_FALLBACK,

    // Misc
    ALIR_OP_CAST,
    ALIR_OP_SIZEOF,
    ALIR_OP_ALIGNOF,
} AlirOpcode;

/**
 * @brief An ALIR instruction.
 */
typedef struct AlirInst {
    AlirOpcode op;
    AlirValue *dest;        // Result (e.g., %1 = ...)
    AlirValue *op1;         // First operand
    AlirValue *op2;         // Second operand (optional)

    // For Calls or Switches
    AlirValue **args;
    int arg_count;
    int custom_flag;

    struct AlirInst *next;

    // Source mapping context
    int line;
    int col;
} AlirInst;

/**
 * @brief A switch case in ALIR.
 */
typedef struct AlirSwitchCase {
    long value;
    char *label;
    struct AlirSwitchCase *next;
} AlirSwitchCase;

/**
 * @brief An edge in the control flow graph.
 */
typedef struct BlockEdge {
    struct AlirBlock *block;
    struct BlockEdge *next;
} BlockEdge;

/**
 * @brief A basic block in ALIR.
 */
typedef struct AlirBlock {
    int id;                 // Block ID (L1, L2...)
    char *label;            // Human readable label
    AlirInst *head;
    AlirInst *tail;
    struct AlirBlock *next;
    BlockEdge *pred;
    BlockEdge *succ;
} AlirBlock;

/**
 * @brief A function parameter in ALIR.
 */
typedef struct AlirParam {
    char *name;
    VarType type;
    struct AlirParam *next;
} AlirParam;

/**
 * @brief A function in ALIR.
 */
typedef struct AlirFunction {
    char *name;
    VarType ret_type;

    // Params
    AlirParam *params;
    int param_count;

    AlirBlock *blocks;
    int block_count;
    int is_flux;
    int is_varargs;
    int is_extern;
    int is_pure;
    char *reason;
    char *cconv;
    struct AlirFunction *next;
} AlirFunction;

// --- EXPLICIT STRUCT & ENUM DEFINITIONS ---

/**
 * @brief A struct field descriptor in ALIR.
 */
typedef struct AlirField {
    char *name;
    VarType type;
    int index;
    struct AlirField *next;
} AlirField;

/**
 * @brief A struct type definition in ALIR.
 */
typedef struct AlirStruct {
    char *name;
    AlirField *fields;
    int field_count;
    int is_union;
    struct AlirStruct *next;
} AlirStruct;

/**
 * @brief An enum entry in ALIR.
 */
typedef struct AlirEnumEntry {
    char *name;
    long value;
    struct AlirEnumEntry *next;
} AlirEnumEntry;

/**
 * @brief An enum type definition in ALIR.
 */
typedef struct AlirEnum {
    char *name;
    AlirEnumEntry *entries;
    struct AlirEnum *next;
} AlirEnum;

/**
 * @brief A global variable or constant in ALIR.
 */
typedef struct AlirGlobal {
    char *name;
    char *string_content; // If string constant
    VarType type;
    struct AlirGlobal *next;
} AlirGlobal;

/**
 * @brief A constant folding entry in ALIR.
 */
typedef struct AlirConstFoldEntry {
    char *name;
    AlirValue *value;
    struct AlirConstFoldEntry *next;
} AlirConstFoldEntry;

/**
 * @brief An ALIR module containing functions, structs, enums, and globals.
 */
typedef struct AlirModule {
    char *name;
    AlirGlobal *globals;    // Global constants (strings)
    AlirFunction *functions;
    AlirStruct *structs;    // Registry of struct definitions
    AlirEnum *enums;        // Registry of enum definitions
    CompilerContext *compiler_ctx; // Reference for Arena
    int str_counter;        // For naming global strings across the module
    AlirConstFoldEntry *const_folds; // Persistent const fold entries

    HashMap const_fold_map;        // name -> AlirValue*

    // Fast lookup maps
    HashMap struct_map;
    HashMap enum_map;
    HashMap func_map;

    // Diagnostics tracing
    const char *src;
    const char *filename;
} AlirModule;

/**
 * @brief A local symbol in ALIR.
 */
typedef struct AlirSymbol {
    char *name;
    AlirValue *ptr;
    VarType type;
    struct AlirSymbol *next;
} AlirSymbol;

/**
 * @brief A flux (generator) variable in ALIR.
 */
typedef struct FluxVar {
    char *name;
    VarType type;
    int index; // Index in the context struct
    struct FluxVar *next;
} FluxVar;

/**
 * @brief The ALIR generation context.
 */
typedef struct AlirCtx {
    SemanticCtx *sem;       // Reference to Semantic Context for Type Resolution

    AlirModule *module;
    AlirFunction *current_func;
    AlirBlock *current_block;

    AlirSymbol *symbols;    // Local IR Symbol Table (Name -> Register)
    HashMap symbol_map;     // Fast lookup for local symbols

    int temp_counter;
    int label_counter;
    int str_counter;        // For naming global strings

    AlirBlock *loop_continue;
    AlirBlock *loop_break;
    struct AlirCtx *loop_parent;

    int in_flux_resume;
    FluxVar *flux_vars;
    AlirValue *flux_ctx_ptr;       // The %ctx pointer in Resume
    char *flux_struct_name;        // Name of the struct
    int flux_yield_count;
    AlirSwitchCase *flux_resume_cases;  // Pending if/else chain for flux dispatch

    int current_line;
    int current_col;

    AlirConstFoldEntry *const_folds;

    HashMap const_fold_map;

    ASTNode **defers;
    int defer_count;
    int defer_capacity;

    HashMap class_map;      // class name -> ClassNode*

    // Fallback block context: when non-NULL, `return` inside a ?{} block stores the value
    // into `fallback_result_slot` and jumps to `fallback_merge_block` instead of exiting the function.
    AlirBlock *fallback_merge_block;
    AlirValue *fallback_result_slot;
    struct AlirCtx *fallback_parent_ctx;
} AlirCtx;

// Struct & Enum Registry

/**
 * @brief Registers a struct type in the module.
 * @param mod The ALIR module.
 * @param name The struct name.
 * @param fields The linked list of fields.
 * @param is_union Whether this is a union.
 */
void alir_register_struct(AlirModule *mod, const char *name, AlirField *fields, int is_union);

/**
 * @brief Finds a struct by name.
 * @param mod The ALIR module.
 * @param name The struct name.
 * @return The struct descriptor, or NULL if not found.
 */
AlirStruct* alir_find_struct(AlirModule *mod, const char *name);

/**
 * @brief Gets the field index within a struct.
 * @param mod The ALIR module.
 * @param struct_name The struct name.
 * @param field_name The field name.
 * @return The field index, or -1 on failure.
 */
int alir_get_field_index(AlirModule *mod, const char *struct_name, const char *field_name);

/**
 * @brief Registers an enum type in the module.
 * @param mod The ALIR module.
 * @param name The enum name.
 * @param entries The linked list of enum entries.
 */
void alir_register_enum(AlirModule *mod, const char *name, AlirEnumEntry *entries);

/**
 * @brief Creates a constant integer ALIR value.
 * @param mod The ALIR module.
 * @param val The integer value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_int(AlirModule *mod, long val);

/**
 * @brief Creates a constant boolean ALIR value.
 * @param mod The ALIR module.
 * @param val The boolean value (0 or 1).
 * @return The created ALIR value.
 */
AlirValue* alir_const_bool(AlirModule *mod, int val);

/**
 * @brief Finds an enum by name.
 * @param mod The ALIR module.
 * @param name The enum name.
 * @return The enum descriptor, or NULL if not found.
 */
AlirEnum* alir_find_enum(AlirModule *mod, const char *name);

/**
 * @brief Gets the value of an enum entry.
 * @param mod The ALIR module.
 * @param enum_name The enum name.
 * @param entry_name The entry name.
 * @param out_val Output parameter for the value.
 * @return 0 on success, non-zero on failure.
 */
int alir_get_enum_value(AlirModule *mod, const char *enum_name, const char *entry_name, long *out_val);

// REQUIRES: Semantic Context (populated via sem_check_program)

/**
 * @brief Generates the complete ALIR module from a semantic context.
 * @param sem The semantic context.
 * @param root The root AST node.
 * @return The generated ALIR module.
 */
AlirModule* alir_generate(SemanticCtx *sem, ASTNode *root);

/**
 * @brief Prints the ALIR module to stderr.
 * @param mod The ALIR module.
 */
void alir_print(AlirModule *mod);

/**
 * @brief Emits the ALIR module to a file.
 * @param mod The ALIR module.
 * @param filename The output file path.
 */
void alir_emit_to_file(AlirModule *mod, const char *filename);

/**
 * @brief Emits an instruction to the current block.
 * @param ctx The ALIR context.
 * @param i The instruction to emit.
 */
void emit(AlirCtx *ctx, AlirInst *i);

/**
 * @brief Creates a new ALIR instruction.
 * @param mod The ALIR module.
 * @param op The opcode.
 * @param dest The destination value, or NULL.
 * @param op1 The first operand.
 * @param op2 The second operand, or NULL.
 * @return The new instruction.
 */
AlirInst* mk_inst(AlirModule *mod, AlirOpcode op, AlirValue *dest, AlirValue *op1, AlirValue *op2);

/**
 * @brief Allocates a new temporary ALIR value.
 * @param ctx The ALIR context.
 * @param t The type of the temporary.
 * @return The new temporary value.
 */
AlirValue* new_temp(AlirCtx *ctx, VarType t);

/**
 * @brief Promotes a value to a target type.
 * @param ctx The ALIR context.
 * @param v The value to promote.
 * @param target The target type.
 * @return The promoted value.
 */
AlirValue* promote(AlirCtx *ctx, AlirValue *v, VarType target);

/**
 * @brief Adds a symbol to the local symbol table.
 * @param ctx The ALIR context.
 * @param name The symbol name.
 * @param ptr The ALIR value pointer.
 * @param t The symbol type.
 */
void alir_add_symbol(AlirCtx *ctx, const char *name, AlirValue *ptr, VarType t);

/**
 * @brief Finds a symbol in the local symbol table.
 * @param ctx The ALIR context.
 * @param name The symbol name.
 * @return The symbol, or NULL if not found.
 */
AlirSymbol* alir_find_symbol(AlirCtx *ctx, const char *name);

/**
 * @brief Generates ALIR for a flux function definition.
 * @param ctx The ALIR context.
 * @param fn The function definition node.
 * @param class_name The class name, or NULL.
 */
void alir_gen_flux_def(AlirCtx *ctx, FuncDefNode *fn, const char *class_name);

/**
 * @brief Generates ALIR for a flux yield expression.
 * @param ctx The ALIR context.
 * @param en The emit node.
 */
void alir_gen_flux_yield(AlirCtx *ctx, EmitNode *en);

/**
 * @brief Recursively collects flux variables from an AST.
 * @param ctx The ALIR context.
 * @param node The AST node.
 * @param idx_ptr Pointer to the current index counter.
 */
void collect_flux_vars_recursive(AlirCtx *ctx, ASTNode *node, int *idx_ptr);

/**
 * @brief Evaluates an AST node as a constant integer.
 * @param ctx The ALIR context.
 * @param node The AST node.
 * @return The evaluated integer value.
 */
long alir_eval_constant_int(AlirCtx *ctx, ASTNode *node);

/**
 * @brief Lowers a new object creation into ALIR.
 * @param ctx The ALIR context.
 * @param class_name The class name.
 * @param args The constructor arguments.
 * @return The resulting ALIR value.
 */
AlirValue* alir_lower_new_object(AlirCtx *ctx, const char *class_name, ASTNode *args);

/**
 * @brief Generates ALIR for a switch statement.
 * @param ctx The ALIR context.
 * @param sn The switch AST node.
 */
void alir_gen_switch(AlirCtx *ctx, SwitchNode *sn);

/**
 * @brief Scans and registers all classes in the AST.
 * @param ctx The ALIR context.
 * @param root The root AST node.
 */
void alir_scan_and_register_classes(AlirCtx *ctx, ASTNode *root);

/**
 * @brief Converts an opcode to its string representation.
 * @param op The opcode.
 * @return The string name of the opcode.
 */
const char* alir_op_str(AlirOpcode op);

/**
 * @brief Allocates memory from the ALIR module's arena.
 * @param mod The ALIR module.
 * @param size The number of bytes to allocate.
 * @return The allocated memory, or NULL on failure.
 */
void* alir_alloc(AlirModule *mod, size_t size);

/**
 * @brief Duplicates a string into the ALIR module's arena.
 * @param mod The ALIR module.
 * @param str The string to duplicate.
 * @return The duplicated string, or NULL on failure.
 */
char* alir_strdup(AlirModule *mod, const char *str);

/**
 * @brief Writes the ALIR module to a binary file.
 * @param mod The ALIR module.
 * @param filename The output file path.
 * @return 0 on success, non-zero on failure.
 */
int alir_write_binary(AlirModule *mod, const char *filename);

/**
 * @brief Reads an ALIR module from a binary file.
 * @param ctx The compiler context.
 * @param filename The input file path.
 * @return The loaded ALIR module, or NULL on failure.
 */
AlirModule* alir_read_binary(CompilerContext *ctx, const char *filename);

#include "lvalue.h"
#include "const.h"
#include "stmt.h"
#include "generator.h"
#include "core.h"
#include "flux.h"

#include "fragment/generate.h"
#include "fragment/addr.h"

#endif // ALIR_H
