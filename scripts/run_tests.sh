#!/bin/bash

# Alkyl Test Runner
# Usage: ./scripts/run_tests.sh [pattern] [--update]

UPDATE=0
PATTERN=""

for arg in "$@"; do
    if [ "$arg" == "--update" ]; then
        UPDATE=1
    else
        PATTERN="$arg"
    fi
done

COLOR_RESET="\033[0m"
COLOR_RED="\033[1;31m"
COLOR_GREEN="\033[1;32m"
COLOR_YELLOW="\033[1;33m"
COLOR_BLUE="\033[1;34m"

# Ensure directories exist
mkdir -p test/logdiff test/diff

FAILED=0
PASSED=0
TOTAL=0

if [ $UPDATE -eq 1 ]; then
    echo -e "${COLOR_YELLOW}Updating expected files...${COLOR_RESET}"
fi

echo -e "${COLOR_BLUE}Starting Alkyl Tests...${COLOR_RESET}"

# Find all .aky files
FILES=$(find test/code -name "*.aky" | sort)

if [ -n "$PATTERN" ]; then
    FILES=$(echo "$FILES" | grep "$PATTERN")
fi

for AKY_FILE in $FILES; do
    # Extract feature and name
    # Path: test/code/FEATURE/NAME.aky
    REL_PATH=${AKY_FILE#test/code/}
    FEATURE=$(dirname "$REL_PATH")
    NAME=$(basename "$REL_PATH" .aky)
    
    TOTAL=$((TOTAL + 1))
    
    EXPECTED_LOG="test/log/$FEATURE/$NAME.log"
    EXPECTED_OUT="test/output/$FEATURE/$NAME.out"
    INPUT_FILE="test/input/$FEATURE/$NAME.in"
    LOGDIFF="test/logdiff/$FEATURE/$NAME.logdiff"
    RUN_DIFF="test/diff/$FEATURE/$NAME.diff"
    
    ACTUAL_LOG="/tmp/alkyl_actual_comp.log"
    ACTUAL_OUT="/tmp/alkyl_actual_run.out"
    
    echo -n "Testing [$FEATURE] $NAME ... "
    
    # 1. Compilation
    ./alkyl "$AKY_FILE" > "$ACTUAL_LOG" 2>&1
    COMP_RET=$?
    
    if [ $UPDATE -eq 1 ]; then
        cp "$ACTUAL_LOG" "$EXPECTED_LOG"
    fi

    # Check compilation log if expected exists
    if [ -f "$EXPECTED_LOG" ]; then
        if ! diff -u "$EXPECTED_LOG" "$ACTUAL_LOG" > "$LOGDIFF"; then
            echo -e "${COLOR_RED}FAILED (Log Mismatch)${COLOR_RESET}"
            FAILED=$((FAILED + 1))
            continue
        else
            rm -f "$LOGDIFF"
        fi
    fi
    
    # 2. Execution (if compiled)
    if [ -f "./out" ]; then
        if [ -f "$INPUT_FILE" ]; then
            ./out < "$INPUT_FILE" > "$ACTUAL_OUT" 2>&1
        else
            ./out > "$ACTUAL_OUT" 2>&1
        fi
        RUN_RET=$?
        
        if [ $UPDATE -eq 1 ]; then
            cp "$ACTUAL_OUT" "$EXPECTED_OUT"
        fi

        # Check output if expected exists
        if [ -f "$EXPECTED_OUT" ]; then
            if ! diff -u "$EXPECTED_OUT" "$ACTUAL_OUT" > "$RUN_DIFF"; then
                echo -e "${COLOR_RED}FAILED (Output Mismatch)${COLOR_RESET}"
                FAILED=$((FAILED + 1))
                rm -f ./out
                continue
            else
                rm -f "$RUN_DIFF"
            fi
        fi
        
        rm -f ./out
    else
        # If no ./out, check if it was supposed to fail
        if [ $COMP_RET -eq 0 ] && [ ! -f "$EXPECTED_LOG" ]; then
             echo -e "${COLOR_RED}FAILED (No executable produced)${COLOR_RESET}"
             FAILED=$((FAILED + 1))
             continue
        fi
    fi
    
    echo -e "${COLOR_GREEN}PASSED${COLOR_RESET}"
    PASSED=$((PASSED + 1))
done

echo "------------------------------------------------"
echo -e "Summary: ${COLOR_GREEN}$PASSED Passed${COLOR_RESET}, ${COLOR_RED}$FAILED Failed${COLOR_RESET} of $TOTAL Total"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
