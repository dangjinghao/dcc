CC := $(CURDIR)/../build/dcc
CFLAGS := -I$(CURDIR)/include
LIBS :=
EXT_CFLAGS := 

TESTS := $(wildcard ./**/*.c)
TEST_EXES := $(TESTS:.c=.out)
TEST_RUNS := $(TESTS:.c=.run)
TEST_CASE := 

.PHONY: test clean run-all FORCE

test:
	@echo "Ensuring dcc is up to date..."
	@cmake --build ../build
	@if [ -n "$(TEST_CASE)" ]; then \
		echo "Running test case: $(TEST_CASE)"; \
		$(MAKE) --no-print-directory $(TEST_CASE); \
	else \
		$(MAKE) --no-print-directory run-all; \
	fi

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

chibicc/%.out: LIBS += -x c $(CURDIR)/chibicc/common.lib

FORCE: