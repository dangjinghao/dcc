#!/bin/bash
repo='https://github.com/rui314/chibicc.git'

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
. "$SCRIPT_DIR/common"

git_checkout 90d1f7f199cc55b13c7fdb5839d1409806633fdb

$make clean
$make CC=$dcc
$make test