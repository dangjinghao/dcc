TARGET := dcc
SRCS := $(shell find . -name "*.c" ! -name "test*")
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)
INCS := libs lexer parser
CFLAGS := -ggdb -Og -MMD $(addprefix -I,$(INCS)) 
CFLAGS += -Wall -Wno-stringop-truncation -Wno-format-truncation -Wno-unused-but-set-variable
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

compile_commands.json: makefile
	@make clean
	@bear -- make build -j

clean:
	@$(RM) $(TARGET) $(OBJS) $(DEPS) test.out

TEST_ENTRY_OBJ := $(TEST_ENTRY:.c=.o)
test: $(OBJS) makefile $(TEST_ENTRY_OBJ)
	@if [ -f "$(TEST_ENTRY)" ]; then \
		$(CC) $(TEST_ENTRY_OBJ) $(OBJS) $(LDFLAGS) -o test.out; \
		./test.out; \
	else \
		echo "No TEST_ENTRY found"; \
	fi
	

.PHONY: run gdb clean test