#!/bin/bash

# Alkyl Test Runner
# Usage: ./scripts/run_tests.sh [pattern] [--update] [--opt] [--unopt] [--llvm|--qbe|--ethyl]
#   --opt    : run only optimized ALIR tests (output: build/opt_out)
#   --unopt  : run only unoptimized ALIR tests (output: build/out)
#   --llvm   : use build/alkyl_llvm as compiler
#   --qbe    : use build/alkyl_qbe as compiler
#   --ethyl  : use build/ethyl as compiler
#   default  : run both (unopt first, then opt) with build/alkyl symlink

UPDATE=0
RUN_OPT=0
RUN_UNOPT=0
COMPILER="build/alkyl"

# Parse the script runner
for arg in "$@"; do
    if [ "$arg" == "--update" ]; then
        UPDATE=1
    elif [ "$arg" == "--opt" ]; then
        RUN_OPT=1
    elif [ "$arg" == "--unopt" ]; then
        RUN_UNOPT=1
    elif [ "$arg" == "--llvm" ]; then
        COMPILER="build/alkyl_llvm"
    elif [ "$arg" == "--qbe" ]; then
        COMPILER="build/alkyl_qbe"
    elif [ "$arg" == "--ethyl" ]; then
        COMPILER="build/ethyl"
    fi
done

# If neither --opt nor --unopt specified, run both
if [ $RUN_OPT -eq 0 ] && [ $RUN_UNOPT -eq 0 ]; then
    RUN_OPT=1
    RUN_UNOPT=1
fi

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

for AKY_FILE in $FILES; do
    # Extract feature and name
    REL_PATH=${AKY_FILE#test/code/}
    FEATURE=$(dirname "$REL_PATH")
    NAME=$(basename "$REL_PATH" .aky)

    EXPECTED_LOG="test/log/$FEATURE/$NAME.log"
    EXPECTED_OUT="test/output/$FEATURE/$NAME.out"
    INPUT_FILE="test/input/$FEATURE/$NAME.in"
    LOGDIFF="test/logdiff/$FEATURE/$NAME.logdiff"
    RUN_DIFF="test/diff/$FEATURE/$NAME.diff"

    ACTUAL_LOG="/tmp/alkyl_actual_comp.log"
    ACTUAL_OUT="/tmp/alkyl_actual_run.out"

    mkdir -p "test/log/$FEATURE" "test/output/$FEATURE" "test/logdiff/$FEATURE" "test/diff/$FEATURE"

    # [NEW] Clean logs (strip ANSI escape codes) for a cleaner diff
    CLEAN_ACTUAL_LOG="/tmp/alkyl_actual_comp_clean.log"
    CLEAN_EXPECTED_LOG="/tmp/alkyl_expected_comp_clean.log"

    # Extract flags if specified on the first line of the file (e.g. // FLAGS: --some-flag)
    FIRST_LINE=$(head -n 1 "$AKY_FILE")
    FLAGS=()
    if [[ "$FIRST_LINE" == "// FLAGS: "* ]]; then
        FLAGS_STR="${FIRST_LINE#// FLAGS: }"
        FLAGS_STR=$(echo "$FLAGS_STR" | tr -d '\r')
        read -r -a FLAGS <<< "$FLAGS_STR"
    fi

    # Determine which modes to run for this test
    MODES=()
    if [ $RUN_UNOPT -eq 1 ]; then
        MODES+=("unopt")
    fi
    if [ $RUN_OPT -eq 1 ]; then
        MODES+=("opt")
    fi

    TEST_PASSED=1

    for MODE in "${MODES[@]}"; do
        if [ "$MODE" == "unopt" ]; then
            MODE_LABEL="unopt"
            COMPILER_FLAGS=("--unopt")
            OUTPUT_BIN="build/out"
        else
            MODE_LABEL="opt"
            COMPILER_FLAGS=("--opt")
            OUTPUT_BIN="build/opt_out"
        fi

        echo -n "${COMPILER} ${COMPILER_FLAGS[*]} $AKY_FILE ... "

        # 1. Compilation
        ${COMPILER} "${COMPILER_FLAGS[@]}" "${FLAGS[@]}" "$AKY_FILE" > "$ACTUAL_LOG" 2>&1
        COMP_RET=$?

        # Strip colors for diffing
        sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]//g" "$ACTUAL_LOG" > "$CLEAN_ACTUAL_LOG"

        if [ $UPDATE -eq 1 ]; then
            if [ $COMP_RET -ne 0 ]; then
                sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]//g" "$ACTUAL_LOG" > "$EXPECTED_LOG"
            else
                if [ -f "$EXPECTED_LOG" ] || [ ! -f "$EXPECTED_OUT" ]; then
                    sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]//g" "$ACTUAL_LOG" > "$EXPECTED_LOG"
                fi
            fi
        fi

        # Check compilation success/failure
        if [ $COMP_RET -ne 0 ]; then
            rm -f "$OUTPUT_BIN"
            echo -e "${COLOR_RED}FAILED ($MODE_LABEL: Compilation failed with exit code $COMP_RET)${COLOR_RESET}"
            FAILED=$((FAILED + 1))
            TEST_PASSED=0
            continue
        fi

        # 2. Execution (if compiled successfully)
        if [ -f "$OUTPUT_BIN" ]; then
            if [ -f "$INPUT_FILE" ]; then
                ./"$OUTPUT_BIN" < "$INPUT_FILE" > "$ACTUAL_OUT" 2>&1
            else
                ./"$OUTPUT_BIN" > "$ACTUAL_OUT" 2>&1
            fi
            RUN_RET=$?

            if [ $RUN_RET -ne 0 ]; then
                echo -e "${COLOR_RED}FAILED ($MODE_LABEL: Execution failed with exit code $RUN_RET)${COLOR_RESET}"
                FAILED=$((FAILED + 1))
                rm -f "$OUTPUT_BIN"
                TEST_PASSED=0
                continue
            fi

            if [ $UPDATE -eq 1 ]; then
                cp "$ACTUAL_OUT" "$EXPECTED_OUT"
            fi

            # Check output if expected exists
            if [ -f "$EXPECTED_OUT" ]; then
                if ! diff "$EXPECTED_OUT" "$ACTUAL_OUT" > "$RUN_DIFF"; then
                    echo -e "${COLOR_RED}FAILED ($MODE_LABEL: Output Mismatch)${COLOR_RESET}"
                    FAILED=$((FAILED + 1))
                    rm -f "$OUTPUT_BIN"
                    TEST_PASSED=0
                    continue
                else
                    rm -f "$RUN_DIFF"
                fi
            fi

            # Check compilation log if expected exists for positive tests
            if [ -f "$EXPECTED_LOG" ]; then
                sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]//g" "$EXPECTED_LOG" > "$CLEAN_EXPECTED_LOG"
                if ! diff "$CLEAN_EXPECTED_LOG" "$CLEAN_ACTUAL_LOG" > "$LOGDIFF"; then
                    echo -e "${COLOR_RED}FAILED ($MODE_LABEL: Log Mismatch)${COLOR_RESET}"
                    FAILED=$((FAILED + 1))
                    rm -f "$OUTPUT_BIN"
                    TEST_PASSED=0
                    continue
                else
                    rm -f "$LOGDIFF"
                fi
            fi

            rm -f "$OUTPUT_BIN"
        fi

        echo -e "${COLOR_GREEN}PASSED ($MODE_LABEL)${COLOR_RESET}"
    done

    if [ $TEST_PASSED -eq 1 ]; then
        PASSED=$((PASSED + 1))
    fi
    TOTAL=$((TOTAL + 1))
done

echo "------------------------------------------------"
echo -e "Summary: ${COLOR_GREEN}$PASSED Passed${COLOR_RESET}, ${COLOR_RED}$FAILED Failed${COLOR_RESET} of $TOTAL Total"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
