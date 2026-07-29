#!/bin/bash

# Alkyl Test Runner
# Usage: ./scripts/run_tests.sh [pattern] [--update] [--opt] [--unopt] [--llvm|--qbe|--ethyl] [--parallel]
#   --opt    : run only optimized ALIR tests (output: build/opt_out)
#   --unopt  : run only unoptimized ALIR tests (output: build/out)
#   --llvm   : use build/alkyl_llvm as compiler
#   --qbe    : use build/alkyl_qbe as compiler
#   --ethyl  : use build/ethyl as compiler
#   --parallel : run tests in parallel (uses NPROC jobs)
#   default  : run both (unopt first, then opt) with build/alkyl symlink

UPDATE=0
RUN_OPT=0
RUN_UNOPT=0
COMPILER="build/alkyl"
PARALLEL=0
CORES=1

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
    elif [ "$arg" == "--parallel" ]; then
        PARALLEL=1
        CORES=$(nproc 2>/dev/null || echo 4)
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
mkdir -p test/log test/output test/logdiff test/diff
mkdir -p build/tmp

RESULT_FILE="build/tmp/test_results.txt"
> "$RESULT_FILE"

if [ $UPDATE -eq 1 ]; then
    echo -e "${COLOR_YELLOW}Updating expected files...${COLOR_RESET}"
fi

echo -e "${COLOR_BLUE}Starting Alkyl Tests...${COLOR_RESET}"

# Function to run a single test - outputs result to stdout
run_single_test() {
    local AKY_FILE="$1"
    local FEATURE="$2"
    local NAME="$3"
    local MODE="$4"
    local COMPILER="$5"
    local UPDATE="$6"

    local EXPECTED_LOG="test/log/$FEATURE/$NAME.log"
    local EXPECTED_OUT="test/output/$FEATURE/$NAME.out"
    local INPUT_FILE="test/input/$FEATURE/$NAME.in"
    local LOGDIFF="test/logdiff/$FEATURE/$NAME.logdiff"
    local RUN_DIFF="test/diff/$FEATURE/$NAME.diff"

    local ACTUAL_LOG="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}_comp.log"
    local ACTUAL_OUT="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}_run.out"
    local CLEAN_ACTUAL_LOG="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}_clean.log"
    local CLEAN_EXPECTED_LOG="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}_expclean.log"
    local OUTPUT_BIN="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}"

    mkdir -p "test/log/$FEATURE" "test/output/$FEATURE" "test/logdiff/$FEATURE" "test/diff/$FEATURE"

    FIRST_LINE=$(head -n 1 "$AKY_FILE")
    FLAGS=()
    if [[ "$FIRST_LINE" == "// FLAGS: "* ]]; then
        FLAGS_STR="${FIRST_LINE#// FLAGS: }"
        FLAGS_STR=$(echo "$FLAGS_STR" | tr -d '\r')
        read -r -a FLAGS <<< "$FLAGS_STR"
    fi

    local COMPILER_FLAGS
    if [ "$MODE" == "unopt" ]; then
        COMPILER_FLAGS="--unopt"
    else
        COMPILER_FLAGS="--opt"
    fi

    ${COMPILER} -o "$OUTPUT_BIN" $COMPILER_FLAGS "${FLAGS[@]}" "$AKY_FILE" > "$ACTUAL_LOG" 2>&1
    local COMP_RET=$?

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

    if [ $COMP_RET -ne 0 ]; then
        rm -f "$OUTPUT_BIN"
        echo "${FEATURE}/${NAME}|${MODE}|FAIL:compilation"
        return
    fi

    if [ -f "$OUTPUT_BIN" ]; then
        if [ -f "$INPUT_FILE" ]; then
            ./"$OUTPUT_BIN" < "$INPUT_FILE" > "$ACTUAL_OUT" 2>&1
        else
            ./"$OUTPUT_BIN" > "$ACTUAL_OUT" 2>&1
        fi
        local RUN_RET=$?

        if [ $RUN_RET -ne 0 ]; then
            echo "${FEATURE}/${NAME}|${MODE}|FAIL:execution"
            rm -f "$OUTPUT_BIN"
            return
        fi

        if [ $UPDATE -eq 1 ]; then
            cp "$ACTUAL_OUT" "$EXPECTED_OUT"
        fi

        if [ -f "$EXPECTED_OUT" ]; then
            if ! diff "$EXPECTED_OUT" "$ACTUAL_OUT" > "$RUN_DIFF"; then
                echo "${FEATURE}/${NAME}|${MODE}|FAIL:output_mismatch"
                rm -f "$OUTPUT_BIN"
                return
            else
                rm -f "$RUN_DIFF"
            fi
        fi

        if [ -f "$EXPECTED_LOG" ]; then
            sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]//g" "$EXPECTED_LOG" > "$CLEAN_EXPECTED_LOG"
            if ! diff "$CLEAN_EXPECTED_LOG" "$CLEAN_ACTUAL_LOG" > "$LOGDIFF"; then
                echo "${FEATURE}/${NAME}|${MODE}|FAIL:log_mismatch"
                rm -f "$OUTPUT_BIN"
                return
            else
                rm -f "$LOGDIFF"
            fi
        fi

        rm -f "$OUTPUT_BIN"
    fi

    echo "${FEATURE}/${NAME}|${MODE}|PASS"
}

export -f run_single_test

# Main execution
FILES=$(find test/code -name "*.aky" | sort)

TOTAL=0
PASS_COUNT=0
FAIL_COUNT=0

if [ $PARALLEL -eq 1 ]; then
    echo -e "${COLOR_YELLOW}Running tests in parallel with $CORES jobs...${COLOR_RESET}"
    
    # For each test, generate the modes to run
    while IFS= read -r AKY_FILE; do
        REL_PATH=${AKY_FILE#test/code/}
        FEATURE=$(dirname "$REL_PATH")
        NAME=$(basename "$REL_PATH" .aky)
        
        MODES=""
        [ $RUN_UNOPT -eq 1 ] && MODES="${MODES} unopt"
        [ $RUN_OPT -eq 1 ] && MODES="${MODES} opt"
        
        for MODE in $MODES; do
            echo "${AKY_FILE}|${FEATURE}|${NAME}|${MODE}|${COMPILER}|${UPDATE}"
        done
    done <<< "$FILES" > build/tmp/test_configs.txt
    
    # Run tests in parallel
    if command -v parallel >/dev/null 2>&1; then
        cat build/tmp/test_configs.txt | parallel --colsep '|' --jobs "$CORES" scripts/run_single.sh {1} {2} {3} {4} {5} {6} >> "$RESULT_FILE"
    else
        # Use xargs + bash for parallelism
        cat build/tmp/test_configs.txt | xargs -P "$CORES" -I {} bash -c '
            IFS="|" read -r AKY_FILE FEATURE NAME MODE COMPILER UPDATE <<< "$1"
            scripts/run_single.sh "$AKY_FILE" "$FEATURE" "$NAME" "$MODE" "$COMPILER" "$UPDATE"
        ' _ {} >> "$RESULT_FILE" 2>&1
    fi
    
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        TOTAL=$((TOTAL + 1))
        if [[ "$line" == *"|PASS" ]]; then
            PASS_COUNT=$((PASS_COUNT + 1))
        else
            FAIL_COUNT=$((FAIL_COUNT + 1))
            echo -e "${COLOR_RED}${line}${COLOR_RESET}"
        fi
    done < "$RESULT_FILE"

    echo "------------------------------------------------"
    echo -e "Summary: ${COLOR_GREEN}$PASS_COUNT Passed${COLOR_RESET}, ${COLOR_RED}$FAIL_COUNT Failed${COLOR_RESET} of $TOTAL Total"

    if [ $FAIL_COUNT -gt 0 ]; then
        exit 1
    fi
    exit 0
else
    # Sequential execution
    FAILED=0
    PASSED=0
    TOTAL=0
    while IFS= read -r AKY_FILE; do
        REL_PATH=${AKY_FILE#test/code/}
        FEATURE=$(dirname "$REL_PATH")
        NAME=$(basename "$REL_PATH" .aky)

        EXPECTED_LOG="test/log/$FEATURE/$NAME.log"
        EXPECTED_OUT="test/output/$FEATURE/$NAME.out"
        INPUT_FILE="test/input/$FEATURE/$NAME.in"
        LOGDIFF="test/logdiff/$FEATURE/$NAME.logdiff"
        RUN_DIFF="test/diff/$FEATURE/$NAME.diff"

        ACTUAL_LOG="build/tmp/alkyl_${FEATURE}_${NAME}_comp.log"
        ACTUAL_OUT="build/tmp/alkyl_${FEATURE}_${NAME}_run.out"
        CLEAN_ACTUAL_LOG="build/tmp/alkyl_${FEATURE}_${NAME}_clean.log"
        CLEAN_EXPECTED_LOG="build/tmp/alkyl_${FEATURE}_${NAME}_expclean.log"
        OUTPUT_BIN="build/tmp/alkyl_${FEATURE}_${NAME}"

        mkdir -p "test/log/$FEATURE" "test/output/$FEATURE" "test/logdiff/$FEATURE" "test/diff/$FEATURE"

        FIRST_LINE=$(head -n 1 "$AKY_FILE")
        FLAGS=()
        if [[ "$FIRST_LINE" == "// FLAGS: "* ]]; then
            FLAGS_STR="${FIRST_LINE#// FLAGS: }"
            FLAGS_STR=$(echo "$FLAGS_STR" | tr -d '\r')
            read -r -a FLAGS <<< "$FLAGS_STR"
        fi

        TEST_PASSED=1

        MODES=()
        [ $RUN_UNOPT -eq 1 ] && MODES+=("unopt")
        [ $RUN_OPT -eq 1 ] && MODES+=("opt")

        for MODE in "${MODES[@]}"; do
            echo -n "${COMPILER} "

            if [ "$MODE" == "unopt" ]; then
                COMPILER_FLAGS="--unopt"
                OUTPUT_BIN_PATH="build/tmp/alkyl_${FEATURE}_${NAME}_unopt"
            else
                COMPILER_FLAGS="--opt"
                OUTPUT_BIN_PATH="build/tmp/alkyl_${FEATURE}_${NAME}_opt"
            fi

            # 1. Compilation
            ${COMPILER} -o "$OUTPUT_BIN_PATH" $COMPILER_FLAGS "${FLAGS[@]}" "$AKY_FILE" > "$ACTUAL_LOG" 2>&1
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
                rm -f "$OUTPUT_BIN_PATH"
                echo ""
                echo -e "${COLOR_RED}FAILED ($MODE: Compilation failed with exit code $COMP_RET)${COLOR_RESET}"
                FAILED=$((FAILED + 1))
                TEST_PASSED=0
                continue
            fi

            # 2. Execution (if compiled successfully)
            if [ -f "$OUTPUT_BIN_PATH" ]; then
                if [ -f "$INPUT_FILE" ]; then
                    ./"$OUTPUT_BIN_PATH" < "$INPUT_FILE" > "$ACTUAL_OUT" 2>&1
                else
                    ./"$OUTPUT_BIN_PATH" > "$ACTUAL_OUT" 2>&1
                fi
                RUN_RET=$?

                if [ $RUN_RET -ne 0 ]; then
                    echo ""
                    echo -e "${COLOR_RED}FAILED ($MODE: Execution failed with exit code $RUN_RET)${COLOR_RESET}"
                    FAILED=$((FAILED + 1))
                    rm -f "$OUTPUT_BIN_PATH"
                    TEST_PASSED=0
                    continue
                fi

                if [ $UPDATE -eq 1 ]; then
                    cp "$ACTUAL_OUT" "$EXPECTED_OUT"
                fi

                # Check output if expected exists
                if [ -f "$EXPECTED_OUT" ]; then
                    if ! diff "$EXPECTED_OUT" "$ACTUAL_OUT" > "$RUN_DIFF"; then
                        echo ""
                        echo -e "${COLOR_RED}FAILED ($MODE: Output Mismatch)${COLOR_RESET}"
                        FAILED=$((FAILED + 1))
                        rm -f "$OUTPUT_BIN_PATH"
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
                        echo ""
                        echo -e "${COLOR_RED}FAILED ($MODE: Log Mismatch)${COLOR_RESET}"
                        FAILED=$((FAILED + 1))
                        rm -f "$OUTPUT_BIN_PATH"
                        TEST_PASSED=0
                        continue
                    else
                        rm -f "$LOGDIFF"
                    fi
                fi

                rm -f "$OUTPUT_BIN_PATH"
            fi

            echo -e "${COLOR_GREEN}PASSED ($MODE)${COLOR_RESET}"
        done

        if [ $TEST_PASSED -eq 1 ]; then
            PASSED=$((PASSED + 1))
        fi
        TOTAL=$((TOTAL + 1))
    done <<< "$FILES"

    echo "------------------------------------------------"
    echo -e "Summary: ${COLOR_GREEN}$PASSED Passed${COLOR_RESET}, ${COLOR_RED}$FAILED Failed${COLOR_RESET} of $TOTAL Total"

    if [ $FAILED -gt 0 ]; then
        exit 1
    fi
    exit 0
fi
