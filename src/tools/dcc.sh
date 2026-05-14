#!/bin/bash

cpp -P -E -w -undef \
  -D_LP64=1 \
  -D__C99_MACRO_WITH_VA_ARGS=1 \
  -D__ELF__=1 \
  -D__LP64__=1 \
  -D__SIZEOF_DOUBLE__=8 \
  -D__SIZEOF_FLOAT__=4 \
  -D__SIZEOF_INT__=4 \
  -D__SIZEOF_LONG_DOUBLE__=8 \
  -D__SIZEOF_LONG_LONG__=8 \
  -D__SIZEOF_LONG__=8 \
  -D__SIZEOF_POINTER__=8 \
  -D__SIZEOF_PTRDIFF_T__=8 \
  -D__SIZEOF_SHORT__=2 \
  -D__SIZEOF_SIZE_T__=8 \
  '-D__SIZE_TYPE__=unsigned long' \
  -D__STDC_HOSTED__=1 \
  -D__STDC_NO_COMPLEX__=1 \
  -D__STDC_UTF_16__=1 \
  -D__STDC_UTF_32__=1 \
  -DSTDC_VERSION=201112L \
  -D__STDC__=1 \
  -D__USER_LABEL_PREFIX__= \
  '-D__alignof__(x)=_Alignof(x)' \
  -D__amd64=1 \
  -D__amd64__=1 \
  -D__const__=const \
  -D__gnu_linux__=1 \
  -D__inline__=inline \
  -D__linux=1 \
  -D__linux__=1 \
  -D__signed__=signed \
  '-D__typeof__(x)=typeof(x)' \
  -D__unix=1 \
  -D__unix__=1 \
  -D__volatile__=volatile \
  -D__x86_64=1 \
  -D__x86_64__=1 \
  -Dlinux=1 \
  -Dunix=1 \
  -D__chibicc__=1 \
  -I./include \
  -I/usr/local/include \
  -I/usr/include/x86_64-linux-gnu \
  -I/usr/include \
    "$@" | ./build/dcc -
