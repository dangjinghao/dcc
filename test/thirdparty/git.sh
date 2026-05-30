#!/bin/bash
repo='https://github.com/git/git.git'

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/common"

git checkout 54e85e7af1ac9e9a92888060d6811ae767fea1bc

$make clean
$make V=1 CC=$dcc test
