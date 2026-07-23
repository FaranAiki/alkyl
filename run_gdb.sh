#!/bin/bash
gdb -batch -ex "break vm.c:425" -ex "run" -ex "print arg->kind" -ex "print arg->val.str_val" -ex "continue" --args build/ethyl << 'INPUT'
link c
extern int puts(char* x)
puts("Hello, World!")
INPUT
