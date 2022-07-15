# How to use 
# To make all binaries: make binaries

# Flags from:
# https://lld.llvm.org/WebAssembly.html

CC = clang
CFLAGS = -O3 -flto -nostdlib --target=wasm32
# If your runtime provides malloc, remove nostdlib and walloc.h
# If your runtime provides memcpy, memmove, memset and memcmp, remove ffreestanding

# Memory must be multiples of 65,536
STACK_SIZE = 8388608 # ~ 8 MB
MEMORY_SIZE = 104857600 # ~100 MB
WLDFLAGS = -Wl,--no-entry -Wl,--import-undefined -Wl,-z,stack-size=$(STACK_SIZE) -Wl,--import-memory -Wl,--initial-memory=$(MEMORY_SIZE) -Wl,--lto-O3

# List of functions that should be callable from javascript
LIB_EXPORTS = bopen bclose
TEST_EXPORTS = testWriteBPuts testReadBgets testWriteBWrite testWriteLongBWrite testReadBRead testFreeLargePage

# Location of all sources (.c files).
SRCS_DIR = src/
# List of non-exporting sources.
SRCS = iobl.c walloc.c memtools.c
# Top-level (exported) sources.
LIBS = library.c
# Top-level (exported) tests.
TESTS = tests.c
# Location to build object files.
BUILD_DIR = build/
# Location to place fully-built libraries.
BIN_DIR = lib/
# Location where test framework lives.
TST_DIR = test/
# Location of includes (.h files).
INC = -I include

# Everything beyond this point is determined from previous declarations, don't modify.

# Library and test export functions.
# This is somewhat tricky - it puts the -Wl,--export= before every exported function.
EXPORT_CMD = -Wl,--export=
LEXPORTS = $(addprefix $(EXPORT_CMD), $(LIB_EXPORTS))
TEXPORTS = $(addprefix $(EXPORT_CMD), $(TEST_EXPORTS))

# All objects to build.
OBJECTS = $(addprefix $(BUILD_DIR), $(SRCS:.c=.o))
# All exports to build. 
# TODO: Allow for .wasm extension to avoid re-building every time.
LIB_BINARIES = $(addprefix $(BIN_DIR), $(LIBS:.c=))
TEST_BINARIES = $(addprefix $(TST_DIR), $(TESTS:.c=))

all: $(LIB_BINARIES) $(TEST_BINARIES)
	cat $(SRCS_DIR)*.js $(TST_DIR)worker.js > $(TST_DIR)test_worker.js

tests: $(TEST_BINARIES)
	cat $(SRCS_DIR)*.js $(TST_DIR)worker.js > $(TST_DIR)test_worker.js

library: $(LIB_BINARIES)

binaries: $(LIB_BINARIES)

clean:
	rm -f $(OBJECTS); 
	rm -f $(LIB_BINARIES); 
	rm -f $(TEST_BINARIES); 

.PHONY: clean tests library binaries all


# Build all the object files.
$(BUILD_DIR)%.o: $(SRCS_DIR)%.c | $(BUILD_DIR:/=)
	$(CC) $(INC) -c $(CFLAGS) $< -o $@

# Create directories if needed.
$(BUILD_DIR:/=):
	mkdir -p $@
$(BIN_DIR:/=): 
	mkdir -p $@
# Test dir should exist, as tests need supporting HTML / JS framework to run.

# Make library binaries.
$(LIB_BINARIES): $(addprefix $(SRCS_DIR), $(LIBS)) $(OBJECTS) | $(BIN_DIR:/=)
	$(CC) $(INC) $(CFLAGS) $(OBJECTS) $(WLDFLAGS) $(LEXPORTS) $(addsuffix .c,$(@:$(BIN_DIR)%=$(SRCS_DIR)%)) -o $(addsuffix .wasm,$@) 

# Make test binaries.
$(TEST_BINARIES): $(addprefix $(SRCS_DIR), $(TESTS)) $(OBJECTS) | $(TST_DIR:/=)
	$(CC) $(INC) $(CFLAGS) $(OBJECTS) $(WLDFLAGS) $(LEXPORTS) $(TEXPORTS) $(addsuffix .c,$(@:$(TST_DIR)%=$(SRCS_DIR)%)) -o $(addsuffix .wasm,$@) 

