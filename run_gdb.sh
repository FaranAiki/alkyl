#!/bin/bash
gdb -q -ex "run" -ex "bt" -ex "frame 2" -ex "p ctx->builder" -ex "p LLVMGetInsertBlock(ctx->builder)" -ex "quit" --args ./build/alkyl_llvm project/wmyl/wmyl.kyl --unopt
