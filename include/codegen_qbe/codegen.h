#ifndef QBE_CODEGEN_H
#define QBE_CODEGEN_H

#include "codegen/codegen.h"

char qbe_type(VarType t);
int qbe_type_size(char qtype);
void print_val(FILE *out, AlirValue *v);

void emit_inst(FILE *out, AlirModule *module, AlirInst *inst, AlirBlock *next_block);
int find_max_temp(AlirModule *module);
int alloc_qbe_temp(void);

extern int s_next_qbe_temp;
extern AlirFunction *s_current_qbe_function;

#endif // QBE_CODEGEN_H
