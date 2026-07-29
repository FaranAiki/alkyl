#!/bin/bash

# Wrapper script for running a single Alkyl test
# Usage: ./scripts/run_single.sh <aky_file> <feature> <name> <mode> <compiler> <update>

AKY_FILE="$1"
FEATURE="$2"
NAME="$3"
MODE="$4"
COMPILER="$5"
UPDATE="$6"

ACTUAL_LOG="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}_comp.log"
ACTUAL_OUT="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}_run.out"
CLEAN_ACTUAL_LOG="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}_clean.log"
CLEAN_EXPECTED_LOG="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}_expclean.log"
OUTPUT_BIN="build/tmp/alkyl_${FEATURE}_${NAME}_${MODE}"

mkdir -p "test/log/$FEATURE" "test/output/$FEATURE" "test/logdiff/$FEATURE" "test/diff/$FEATURE"

FIRST_LINE=$(head -n 1 "$AKY_FILE")
FLAGS=()
if [[ "$FIRST_LINE" == "// FLAGS: "* ]]; then
    FLAGS_STR="${FIRST_LINE#// FLAGS: }"
    FLAGS_STR=$(echo "$FLAGS_STR" | tr -d '\r')
    read -r -a FLAGS <<< "$FLAGS_STR"
fi

COMPILER_FLAGS="--unopt"
if [ "$MODE" == "opt" ]; then
    COMPILER_FLAGS="--opt"
fi

EXPECTED_LOG="test/log/$FEATURE/$NAME.log"
EXPECTED_OUT="test/output/$FEATURE/$NAME.out"
INPUT_FILE="test/input/$FEATURE/$NAME.in"
LOGDIFF="test/logdiff/$FEATURE/$NAME.logdiff"
RUN_DIFF="test/diff/$FEATURE/$NAME.diff"

echo -ne "${COMPILER} ${AKY_FILE} (${MODE}): Compiling..."

${COMPILER} -o "$OUTPUT_BIN" $COMPILER_FLAGS "${FLAGS[@]}" "$AKY_FILE" > "$ACTUAL_LOG" 2>&1
COMP_RET=$?

sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]//g" "$ACTUAL_LOG" > "$CLEAN_ACTUAL_LOG"

if [ "$UPDATE" == "1" ]; then
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
    echo "[${COMPILER}] ${AKY_FILE} (${MODE}): ${COLOR_RED}FAIL:compilation${COLOR_RESET}"
    exit 0
fi

if [ -f "$OUTPUT_BIN" ]; then
    if [ -f "$INPUT_FILE" ]; then
        ./"$OUTPUT_BIN" < "$INPUT_FILE" > "$ACTUAL_OUT" 2>&1
    else
        ./"$OUTPUT_BIN" > "$ACTUAL_OUT" 2>&1
    fi
    RUN_RET=$?

    if [ $RUN_RET -ne 0 ]; then
        echo "[${COMPILER}] ${AKY_FILE} (${MODE}): ${COLOR_RED}FAIL:execution${COLOR_RESET}"
        rm -f "$OUTPUT_BIN"
        exit 0
    fi

    if [ "$UPDATE" == "1" ]; then
        cp "$ACTUAL_OUT" "$EXPECTED_OUT"
    fi

    if [ -f "$EXPECTED_OUT" ]; then
        if ! diff "$EXPECTED_OUT" "$ACTUAL_OUT" > "$RUN_DIFF"; then
            echo "[${COMPILER}] ${AKY_FILE} (${MODE}): ${COLOR_RED}FAIL:output_mismatch${COLOR_RESET}"
            rm -f "$OUTPUT_BIN"
            exit 0
        else
            rm -f "$RUN_DIFF"
        fi
    fi

    if [ -f "$EXPECTED_LOG" ]; then
        sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]//g" "$EXPECTED_LOG" > "$CLEAN_EXPECTED_LOG"
        if ! diff "$CLEAN_EXPECTED_LOG" "$CLEAN_ACTUAL_LOG" > "$LOGDIFF"; then
            echo "[${COMPILER}] ${AKY_FILE} (${MODE}): ${COLOR_RED}FAIL:log_mismatch${COLOR_RESET}"
            rm -f "$OUTPUT_BIN"
            exit 0
        else
            rm -f "$LOGDIFF"
        fi
    fi

    rm -f "$OUTPUT_BIN"
fi

echo "[${COMPILER}] ${AKY_FILE} (${MODE}): ${COLOR_GREEN}PASS${COLOR_RESET}"
