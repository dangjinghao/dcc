CC := $(CURDIR)/../build/dcc
CFLAGS := -I$(CURDIR)/include
LIBS :=
TEST_PATH := $(CURDIR)
EXT_CFLAGS := 

TESTS := $(wildcard $(TEST_PATH)/**/*.c)
TEST_EXES := $(TESTS:.c=.out)
TEST_RUNS := $(TESTS:.c=.run)

.PHONY: test clean run-all FORCE

test:
	@echo "Ensuring dcc is up to date..."
	@cmake --build ../build
	@$(MAKE) --no-print-directory run-all

run-all: $(TEST_RUNS)
	@echo "All tests completed!"

.PRECIOUS: %.out

%.out: %.c $(CC)
	@echo "Compiling $<..."
	@$(CC) -o $@ $< $(LIBS) $(CFLAGS) $(EXT_CFLAGS)

%.run: %.out FORCE
	@echo "Running $<..."
	@$<  2>&1

clean:
	@echo "Cleaning test outputs..."
	rm -f $(TEST_EXES)

chibicc/commonsym.out: CFLAGS += -fcommon

chibicc/%.out: LIBS += $(CURDIR)/chibicc/common.lib

FORCE: