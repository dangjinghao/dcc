#!/bin/bash
set +e
if [ -z "$2" ]; then
    echo "Error: parameter is not completed."
    echo "Usage: $0 <input_file> <output_file>"
    exit 1
fi
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"

LIBS_DIR="$SCRIPT_DIR/libs/stdinc"

clang -I"$LIBS_DIR" -nostdinc -E "$1" | grep -v -E "^#.*" | $SCRIPT_DIR/dcc | clang -x ir - -o $2 "${@:3}"
