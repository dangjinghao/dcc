CC := $(CURDIR)/../build/dcc
CFLAGS := -I$(CURDIR)/include
TEST_PATH := $(CURDIR)
EXT_CFLAGS := 

TESTS := $(wildcard $(TEST_PATH)/**/*.c)
TEST_OUTPUTS := $(TESTS:.c=.out)
TEST_RUNS := $(TESTS:.c=.run)


$(CC):
	@echo "Building dcc..."
	@cmake -S .. -B ../build
	@cmake --build ../build

test: $(TEST_RUNS)
	@echo "All tests completed!"

%.out: %.c $(CC)
	@echo "Compiling $<..."
	$(CC) -o $@ $< $(CFLAGS) $(EXT_CFLAGS)

%.run: %.out
	@echo "Running $<..."
	@$<

clean:
	@echo "Cleaning test outputs..."
	@rm -f $(TEST_OUTPUTS)

.PHONY: test clean %.run $(CC)