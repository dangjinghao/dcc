MAKEFLAGS ?= -j $(shell nproc)

TARGET := dcc
SRCS := $(shell find . -name "*.c" ! -name "test*")
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)
INCS := libs lexer parser 

FEATURES := USE_SWITCH_ALGO_BSEARCH

CFLAGS := -ggdb -Og -MMD -std=gnu2x -Wall -Wextra \
		$(addprefix -I,$(INCS)) \
		$(shell llvm-config --cflags) \
		$(addprefix -D,$(FEATURES)) \

CFLAGS += -Wno-stringop-truncation \
		  -Wno-format-truncation \
		  -Wno-unused-parameter \
		  -Wno-unused-function
LDFLAGS := $(shell llvm-config --libs --ldflags)


ARGS :=

-include $(DEPS)

run: $(TARGET)
	./$(TARGET) $(ARGS)

%.o:%.c makefile
	@echo "$(<) --> $(@)" >&2
	@$(CC) $(CFLAGS) -c $(<) -o $(@)

$(TARGET): $(OBJS) makefile
	@echo "linking..." >&2
	@$(CC) $(OBJS) -o $(@) $(LDFLAGS)

build: $(TARGET)

gdb: $(TARGET)
	@gdb $(TARGET)

compile_commands.json: makefile $(SRCS)
	@make clean
	@bear -- make build -j

clean:
	@$(RM) $(shell find -name "*.o") $(shell find -name "*.d") test.out

ifneq (,$(filter test,$(MAKECMDGOALS)))
TEST_ENTRY ?= test.c:main
TEST_FILE := $(word 1, $(subst :, ,$(TEST_ENTRY)))
TEST_FILE_OBJ := $(TEST_FILE:.c=.o)
TEST_ENTRY_FUNC := $(word 2, $(subst :, ,$(TEST_ENTRY)))

ifeq ($(wildcard $(TEST_FILE)),)
$(error "TEST_FILE: $(TEST_FILE) not found")
endif
endif

test: $(OBJS) makefile $(TEST_FILE_OBJ)

	@$(CC) $(TEST_FILE_OBJ) $(OBJS) $(LDFLAGS) -o test.out -Wl,--defsym=main=$(TEST_ENTRY_FUNC)
	@if [ -z "$(RUN)" -a "$(shell echo "$(TEST_ENTRY)"|grep "^parser")" ] ; then \
		$(RUN) ./test.out > analysis/data.json ; \
	else \
		$(RUN) ./test.out ; \
	fi

.PHONY: run gdb clean test