# Makefile for testing the dcc compiler
# Copyright (C) 2025 by dangjinghao (dangjinghaoemail@163.com)
TOP := ..
TEST_FILES := $(wildcard *.c)
CC := $(TOP)/dcc.sh
ARGS =

test: $(TEST_FILES:.c=.output)
	@echo "All tests completed."

31_args.output : ARGS = arg1 arg2 arg3 arg4 arg5

%.test: %.c
	@echo "$* compiling"
	@$(CC) $< -o $*.test -lm 

%.output: %.test %.expect
	@echo "$* testing"
	@./$*.test $(ARGS) > $*.output
	@diff -Nbu $*.output $*.expect
	@echo "$* passed"
	@$(RM) $*.test $*.output

clean:
	@$(RM) $(TEST_FILES:.c=.output) $(TEST_FILES:.c=.test)

.PHONY: test clean
