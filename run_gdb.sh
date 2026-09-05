#!/bin/bash
gdb -batch -ex "run" -ex "bt" --args ./build/alkyl_llvm project/wmyl/wmyl.kyl -o build/wmyl_test_run4
