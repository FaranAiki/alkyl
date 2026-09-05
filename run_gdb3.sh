#!/bin/bash
gdb -batch -ex "b src/driver/main.c:408" -ex "run" -ex "p final_ret" -ex "c" --args ./build/alkyl_llvm project/wmyl/wmyl.kyl -o build/wmyl_test_run2
