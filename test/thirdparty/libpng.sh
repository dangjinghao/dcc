#!/bin/bash
repo='https://github.com/rui314/libpng.git'

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
. "$SCRIPT_DIR/common"

git_checkout dbe3e0c43e549a1602286144d94b0666549b18e6

CC=$dcc ./configure
sed -i 's/^wl=.*/wl=-Wl,/; s/^pic_flag=.*/pic_flag=-fPIC/' libtool
$make clean
$make
$make test
