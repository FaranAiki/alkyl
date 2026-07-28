#!/usr/bin/env python3
with open('/home/faranaiki/Git/alkyl/src/optlir/local.c', 'r') as f:
    content = f.read()

old_merge = '''static int merge_entry_jump_function(AlirModule *module, AlirFunction *func) {
    if (!func || !func->blocks) return 0;

    {
        AlirBlock *b = func->blocks;
        while (b) {
            free_edges(b->pred);
            free_edges(b->succ);
            b->pred = NULL;
            b->succ = NULL;
            b = b->next;
        }
        build_pred_succ(func);
    }

    AlirBlock *entry = func->blocks;
    if (!entry || !entry->head) return 0;

    AlirInst *term = entry->tail;
    if (term->op != ALIR_OP_JUMP || term->op1 == NULL || term->op1->kind != ALIR_VAL_LABEL) return 0;

    const char *target_label = term->op1->val.str_val;
    AlirBlock *target = find_block_by_label(func, target_label);
    if (!target || target == entry) return 0;

    int pred_count = 0;
    for (BlockEdge *e = target->pred; e; e = e->next) pred_count++;
    if (pred_count != 1) return 0;

    if (term != entry->head) {
        AlirInst *first = entry->head;
        AlirInst *last_non_term = NULL;
        AlirInst *cur = first;
        while (cur && cur != term) {
            last_non_term = cur;
            cur = cur->next;
        }
        if (last_non_term) {
            last_non_term->next = NULL;

            if (target->head) {
                last_non_term->next = target->head;
            } else {
                target->tail = last_non_term;
            }
            target->head = first;
        }
    }

    if (strcmp(entry->label, target_label) != 0) {
        redirect_label_in_all_blocks(module, func, entry->label, target_label);
    }

    if (func->blocks == entry) {
        func->blocks = entry->next;
    } else {
        AlirBlock *prev_entry = func->blocks;
        while (prev_entry && prev_entry->next != entry) prev_entry = prev_entry->next;
        if (prev_entry) prev_entry->next = entry->next;
    }
    func->block_count--;

    free_edges(entry->pred);
    free_edges(entry->succ);

    return 1;
}'''

new_merge = '''static void merge_entry_jump_function(AlirModule *module, AlirFunction *func) {
    if (!func || !func->blocks) return;
    
    AlirBlock *entry = func->blocks;
    if (!entry || !entry->head) return;
    
    AlirInst *i = entry->head;
    if (i->op != ALIR_OP_JUMP || i->op1 == NULL || i->op1->kind != ALIR_VAL_LABEL) return;
    if (i->next != NULL) return;
    
    const char *target_label = i->op1->val.str_val;
    AlirBlock *target = find_block_by_label(func, target_label);
    if (!target) return;
    
    AlirBlock *prev = NULL;
    AlirBlock *b = func->blocks;
    while (b && b != target) {
        prev = b;
        b = b->next;
    }
    if (b != target) return;
    
    int pred_count = 0;
    AlirBlock *check = func->blocks;
    while (check) {
        AlirInst *inst = check->head;
        while (inst) {
            if (inst->op == ALIR_OP_JUMP && inst->op1 && inst->op1->kind == ALIR_VAL_LABEL && strcmp(inst->op1->val.str_val, target_label) == 0) {
                pred_count++;
            } else if (inst->op == ALIR_OP_CONDI && inst->op2 && inst->op2->kind == ALIR_VAL_LABEL && strcmp(inst->op2->val.str_val, target_label) == 0) {
                pred_count++;
            } else if (inst->op == ALIR_OP_CONDI && inst->arg_count > 0 && inst->args[0] && inst->args[0]->kind == ALIR_VAL_LABEL) {
                pred_count++;
            } else if (inst->op == ALIR_OP_CONDI && inst->arg_count > 0 && inst->args[0] && inst->args[0]->kind == ALIR_VAL_LABEL && strcmp(inst->args[0]->val.str_val, target_label) == 0) {
                pred_count++;
            }
            inst = inst->next;
        }
        check = check->next;
    }
    
    if (pred_count != 1) return;
    
    if (func->blocks == entry) {
        func->blocks = entry->next;
    } else {
        AlirBlock *prev_entry = func->blocks;
        while (prev_entry && prev_entry->next != entry) prev_entry = prev_entry->next;
        if (prev_entry) prev_entry->next = entry->next;
    }
    func->block_count--;
    
    AlirBlock *new_entry = target;
    while (new_entry) {
        AlirInst *inst = new_entry->head;
        while (inst) {
            if (inst->op == ALIR_OP_JUMP && inst->op1 && inst->op1->kind == ALIR_VAL_LABEL) {
                AlirBlock *t = find_block_by_label(func, inst->op1->val.str_val);
                if (!t) {
                    inst->op1 = alir_val_label(module, "merge");
                }
            } else if (inst->op == ALIR_OP_CONDI) {
                if (inst->op2 && inst->op2->kind == ALIR_VAL_LABEL) {
                    AlirBlock *t = find_block_by_label(func, inst->op2->val.str_val);
                    if (!t) inst->op2 = alir_val_label(module, "merge");
                }
                if (inst->arg_count > 0 && inst->args[0] && inst->args[0]->kind == ALIR_VAL_LABEL) {
                    AlirBlock *t = find_block_by_label(func, inst->args[0]->val.str_val);
                    if (!t) inst->args[0] = alir_val_label(module, "merge");
                }
            }
            inst = inst->next;
        }
        new_entry = new_entry->next;
    }
}'''

assert old_merge in content, "Could not find current merge function"
content = content.replace(old_merge, new_merge)

with open('/home/faranaiki/Git/alkyl/src/optlir/local.c', 'w') as f:
    f.write(content)
print("Restored original merge function")
