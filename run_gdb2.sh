#!/bin/bash
gdb -batch -ex "b main" -ex "run" -ex "finish" -ex "p \$rax" --args ./build/alkyl_llvm project/wmyl/wmyl.kyl -o build/wmyl_test_run2
