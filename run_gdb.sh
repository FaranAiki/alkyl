#!/bin/bash
export WLR_BACKENDS=headless
gdb -batch -ex "run" -ex "bt" --args ./build/out
