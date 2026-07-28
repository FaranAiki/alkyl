 #ifndef OPTLIR_LOCAL_INTERNAL_H
#define OPTLIR_LOCAL_INTERNAL_H

#include "optlir.h"
#include "optlir/local.h"

// Block set
typedef struct BlockSet {
    AlirBlock **blocks;
    int count;
    int capacity;
} BlockSet;

// Eval helpers
ConstVal get_const_for_value(AlirValue *val);
ConstVal eval_const_binary(int op, ConstVal l, ConstVal r, VarType type);
ConstVal eval_const_unary(int op, ConstVal v, VarType type);
int is_identity_op(int op, ConstVal c);
int is_self_cancel_op(int op);
int all_args_const(AlirInst *inst);
int is_temp_used_except_in_load(AlirFunction *func, AlirValue *temp, AlirInst *exclude_load);

// Constant propagation
void constant_propagate_function(AlirModule *module, AlirFunction *func);
void propagate_param_copies_function(AlirModule *module, AlirFunction *func);
void eval_pure_call_function(AlirModule *module, AlirFunction *func);

// Branch folding
void fold_branches_function(AlirModule *module, AlirFunction *func);

// Block set
void block_set_init(BlockSet *set, int capacity);
void block_set_add(BlockSet *set, AlirBlock *b);
int block_set_has(BlockSet *set, AlirBlock *b);
void block_set_free(BlockSet *set);
AlirBlock* find_block_by_label(AlirFunction *func, const char *label);
void build_pred_succ(AlirFunction *func);

// Unreachable blocks
void mark_reachable_blocks(AlirFunction *func, BlockSet *reachable);
void remove_unreachable_blocks_function(AlirModule *module, AlirFunction *func);

// Merge
void redirect_label_to(AlirModule *module, AlirBlock *b, const char *old_label, const char *new_label);
void redirect_label_in_all_blocks(AlirModule *module, AlirFunction *func, const char *old_label, const char *new_label);
int merge_entry_jump_function(AlirModule *module, AlirFunction *func);

// Dead stores
void remove_instruction(AlirBlock *block, AlirInst *prev, AlirInst *inst);
void free_edges(BlockEdge *e);
int value_is_used_somewhere(AlirFunction *func, AlirValue *val);
void remove_dead_stores_function(AlirModule *module, AlirFunction *func);

#endif
