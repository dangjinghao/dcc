#!/bin/bash
repo='https://github.com/sqlite/sqlite.git'

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
. "$SCRIPT_DIR/common"

git_checkout 86f477edaa17767b39c7bae5b67cac8580f7a8c1

CC=$dcc CFLAGS=-D_GNU_SOURCE ./configure
sed -i 's/^wl=.*/wl=-Wl,/; s/^pic_flag=.*/pic_flag=-fPIC/' libtool
$make clean
$make
$make test
