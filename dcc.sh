#!/bin/bash
set -e

INPUT_FILE=""
OUTPUT_FILE=""
EXPAND_ONLY=0
COMPILE_ONLY=0
EXTRA_ARGS=("-Wno-unused-command-line-argument")

while [[ $# -gt 0 ]]; do
    case "$1" in
        -o)
            if [[ -z "$2" || "$2" == -* ]]; then
                echo "Error: -o requires an argument" >&2
                exit 1
            fi
            OUTPUT_FILE="$2"
            shift 2
            ;;
        -E)
            EXPAND_ONLY=1
            shift
            ;;
        -S)
            COMPILE_ONLY=1
            shift
            ;;
        -*)
            EXTRA_ARGS+=("$1")
            shift
            ;;
        *)
            if [[ -z "$INPUT_FILE" ]]; then
                INPUT_FILE="$1"
            else
                EXTRA_ARGS+=("$1")
            fi
            shift
            ;;
    esac
done

if [[ -z "$INPUT_FILE" ]]; then
    echo "Error: No input file specified." >&2
    echo "Usage: $0 [options] <input_file>" >&2
    echo "Options:" >&2
    echo "  -o <file>    Specify final output file" >&2
    echo "  -E           Preprocess and expand macros" >&2
    echo "  -S           Compile to LLVM IR" >&2
    exit 1
fi

if [[ -z "$OUTPUT_FILE" && $EXPAND_ONLY -eq 0 && $COMPILE_ONLY -eq 0 ]]; then
    echo "Error: No output file specified." >&2
    echo "Use -o option to specify output file." >&2
    exit 1
fi

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
LIBS_DIR="$SCRIPT_DIR/libs/stdinc"

PREPROCESSED=$(clang -I"$LIBS_DIR" -nostdinc -E "$INPUT_FILE" "${EXTRA_ARGS[@]}" | grep -v -E "^#.*")

if [[ $EXPAND_ONLY -eq 1 ]]; then
    echo "$PREPROCESSED"
    exit 0
fi

DCC_OUTPUT=$(echo "$PREPROCESSED" | "$SCRIPT_DIR/dcc")

if [[ $COMPILE_ONLY -eq 1 ]]; then
    echo "$DCC_OUTPUT"
    exit 0
fi

echo "$DCC_OUTPUT" | clang -x ir - -o "$OUTPUT_FILE" "${EXTRA_ARGS[@]}"
