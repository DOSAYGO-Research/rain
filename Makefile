# Compiler and Tools
CXX = /opt/homebrew/opt/llvm/bin/clang++
EMCC = emcc

# Flags
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -O3 -march=native -mtune=native
#CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -O0 -fsanitize=address,undefined -march=native
#CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -O0 -g 
CXXFLAGS += -isysroot $(shell xcrun --show-sdk-path)
CXXFLAGS += -fopenmp -I/opt/homebrew/opt/llvm/include
DEPFLAGS = -MMD -MF $(@:.o=.d)

LDFLAGS = -fopenmp -L/opt/homebrew/opt/llvm/lib -lz -lc++

# Emscripten Flags for WASM
# Include your new bridging funcs in EXPORTED_FUNCTIONS:
EMCCFLAGS = -O2 -s WASM=1 \
  -s EXPORTED_FUNCTIONS="['_rainbowHash64','_rainbowHash128','_rainbowHash256','_rainstormHash64', '_rainstormHash128', '_rainstormHash256', '_rainstormHash512', 'stringToUTF8','UTF8ToString', 'lengthBytesUTF8','_malloc','_free','_wasmGetFileHeaderInfo','_wasmFree', '_wasmStreamEncryptBuffer', '_wasmStreamDecryptBuffer', '_wasmFreeBuffer', '_wasmCreateHMAC', '_wasmVerifyHMAC']" \
  -s EXPORTED_RUNTIME_METHODS="['wasmExports','ccall','cwrap', 'getValue']" \
  -s WASM_BIGINT=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
	-s USE_ZLIB=1 \
	#-s NO_DISABLE_EXCEPTION_CATCHING \
  #-g \
	-flto 

# Directories
OBJDIR = rain/obj
BUILDDIR = rain/bin
WASMDIR = js/wasm

# Sources and Outputs
SRCS = $(wildcard src/*.cpp)
OBJS = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))
DEPS = $(OBJS:.o=.d)

STORM_WASM_SOURCE = src/rainstorm.cpp
BOW_WASM_SOURCE   = src/rainbow.cpp
HEADER_WASM_SOURCE = src/wasm/exports.cpp  # or wherever your wasm bridging is
WASM_OUTPUT = docs/rain.wasm
JS_OUTPUT   = docs/rain.cjs

# Default Target
all: directories node_modules rainsum link rainwasm

# Create Necessary Directories
directories: ${OBJDIR} ${BUILDDIR} ${WASMDIR}

${OBJDIR}:
	mkdir -p ${OBJDIR}

${BUILDDIR}:
	mkdir -p ${BUILDDIR}

${WASMDIR}:
	mkdir -p ${WASMDIR}

# Install Node Modules
node_modules:
	@(test ! -d ./js/node_modules && cd js && npm i && cd ..) || :
	@(test ! -d ./scripts/node_modules && cd scripts && npm i && cd ..) || :

# Build Executable (C++ native)
rainsum: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(BUILDDIR)/$@ $^

# Compile Object Files
$(OBJDIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# Build WebAssembly Output
rainwasm: $(WASM_OUTPUT) $(JS_OUTPUT)

# NOTE: Add your bridging source(s) to the compile command:
$(WASM_OUTPUT) $(JS_OUTPUT): $(STORM_WASM_SOURCE) $(BOW_WASM_SOURCE) $(HEADER_WASM_SOURCE)
	@[ -d docs ] || mkdir -p docs
	@[ -d ${WASMDIR} ] || mkdir -p ${WASMDIR}
	$(EMCC) $(EMCCFLAGS) -o docs/rain.html $^
	mv docs/rain.js $(JS_OUTPUT)
	cp $(WASM_OUTPUT) $(JS_OUTPUT) ${WASMDIR}
	rm docs/rain.html

# Symlink for Convenience
link:
	@ln -sf rain/bin/rainsum

# Installation
.PHONY: install
install: rainsum
	cp $(BUILDDIR)/rainsum /usr/local/bin/

# Include Dependencies
-include $(DEPS)

# Clean Build Artifacts
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(BUILDDIR) rainsum \
	  $(WASMDIR) js/node_modules scripts/node_modules \
	  $(WASM_OUTPUT) $(JS_OUTPUT)

