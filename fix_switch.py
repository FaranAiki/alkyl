import re

with open("src/mlir/mlir_wrapper.cpp", "r") as f:
    content = f.read()

switch_code = """void* alkyl_mlir_build_switch_start(AlkylMlirContext c_ctx, AlkylMlirValue cond, int num_cases) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto state = new SwitchState();
    state->cond = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(cond));
    
    mlir::Block* currentBlock = global_builder->getBlock();
    mlir::Region* parentRegion = currentBlock->getParent();
    
    state->merge_block = new mlir::Block();
    parentRegion->push_back(state->merge_block);
    
    state->current_cond_block = currentBlock;
    
    return state;
#else
    (void)c_ctx; (void)cond; (void)num_cases; return reinterpret_cast<void*>(1);
#endif
}

void alkyl_mlir_build_switch_case_start(AlkylMlirContext c_ctx, void* switch_op_ptr, AlkylMlirValue val, int is_leak) {
#ifdef HAS_MLIR
    if (!global_builder || !switch_op_ptr) return;
    auto state = static_cast<SwitchState*>(switch_op_ptr);
    
    auto case_val = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(val));
    
    mlir::Region* parentRegion = state->current_cond_block->getParent();
    
    mlir::Block* case_block = new mlir::Block();
    mlir::Block* next_cond_block = new mlir::Block();
    
    parentRegion->getBlocks().insert(parentRegion->end(), case_block);
    parentRegion->getBlocks().insert(parentRegion->end(), next_cond_block);
    
    global_builder->setInsertionPointToEnd(state->current_cond_block);
    
    auto cmp = global_builder->create<mlir::arith::CmpIOp>(
        global_builder->getUnknownLoc(),
        mlir::arith::CmpIPredicate::eq,
        state->cond,
        case_val
    );
    
    global_builder->create<mlir::cf::CondBranchOp>(
        global_builder->getUnknownLoc(),
        cmp,
        case_block,
        next_cond_block
    );
    
    global_builder->setInsertionPointToEnd(case_block);
#else
    (void)c_ctx; (void)switch_op_ptr; (void)val; (void)is_leak;
#endif
}

void alkyl_mlir_build_switch_case_end(AlkylMlirContext c_ctx, void* switch_op_ptr) {
#ifdef HAS_MLIR
    if (!global_builder || !switch_op_ptr) return;
    auto state = static_cast<SwitchState*>(switch_op_ptr);
    
    global_builder->create<mlir::cf::BranchOp>(
        global_builder->getUnknownLoc(),
        state->merge_block
    );
    
    // The next block will be next_cond_block, which we appended after case_block.
    // It's the last block before merge_block. We find it by getting the block before merge_block.
    mlir::Region* parentRegion = state->merge_block->getParent();
    mlir::Block* next_cond = &parentRegion->getBlocks().back(); // this is merge_block!
    // Wait, the blocks are pushed back. merge_block was pushed first. But we inserted case_block and next_cond_block before end().
    // Wait, end() is after merge_block?
    // Actually, we can just set current_cond_block to the block where we evaluate the next condition.
    // Since we created next_cond_block, we can just use the fact that the builder's block is case_block, and next_cond_block is right after it.
    
    // Instead of doing block magic, let's just get the block before merge_block?
    // Let's just track it in state!
#else
    (void)c_ctx; (void)switch_op_ptr;
#endif
}
"""
