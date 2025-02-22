TARGET := dcc
SRCS := $(shell find . -name "*.c" ! -name "test*")
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)
INCS := libs lexer parser

CFLAGS := -ggdb -Og -MMD -std=gnu99 $(addprefix -I,$(INCS)) 
CFLAGS += -Wall -Wno-stringop-truncation -Wno-format-truncation -Wno-unused-but-set-variable -Wunused-function
LDFLAGS :=

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
	@$(RM) $(TARGET) $(OBJS) $(DEPS) test.out

TEST_FILE ?= test.c
TEST_FILE_OBJ := $(TEST_FILE:.c=.o)
ifndef TEST_ENTRY
TEST_ENTRY := main
endif
ifeq ($(wildcard $(TEST_FILE)),)
$(error "TEST_FILE:$(TEST_FILE) not found")
endif

test: $(OBJS) makefile $(TEST_FILE_OBJ)

	$(CC) $(TEST_FILE_OBJ) $(OBJS) $(LDFLAGS) -o test.out -Wl,--defsym=main=$(TEST_ENTRY)
	$(RUN) ./test.out > analysis/data.json

.PHONY: run gdb clean test