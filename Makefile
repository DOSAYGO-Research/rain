# Compiler and Tools
CXX = /opt/homebrew/opt/llvm/bin/clang++
EMCC = emcc

# Flags
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -O3 -march=native
CXXFLAGS += -isysroot $(shell xcrun --show-sdk-path)
CXXFLAGS += -fsanitize=thread -fopenmp -I/opt/homebrew/opt/llvm/include
CXXFLAGS += -I/opt/homebrew/opt/zlib/include
DEPFLAGS = -MMD -MF $(@:.o=.d)

LDFLAGS = -fopenmp -L/opt/homebrew/opt/llvm/lib -L/opt/homebrew/opt/zlib/lib -lz -lc++

# Emscripten Flags for WASM
EMCCFLAGS = -O3 -s WASM=1 \
            -s EXPORTED_FUNCTIONS="['_rainbowHash64', '_rainbowHash128', '_rainbowHash256', \
            '_rainbow_rpHash64', '_rainbow_rpHash128', '_rainbow_rpHash256', \
            '_rainstormHash64', '_rainstormHash128', '_rainstormHash256', '_rainstormHash512', \
            '_rainstorm_nis2_v1Hash64', '_rainstorm_nis2_v1Hash128', '_rainstorm_nis2_v1Hash256', '_rainstorm_nis2_v1Hash512', \
            'stringToUTF8', 'lengthBytesUTF8', '_malloc', '_free']" \
            -s EXPORTED_RUNTIME_METHODS="['wasmExports', 'ccall', 'cwrap']" \
            -s WASM_BIGINT=1 -s ALLOW_MEMORY_GROWTH=1 -g 

# Directories
OBJDIR = rain/obj
BUILDDIR = rain/bin
WASMDIR = js/wasm
DOCSDIR = docs

# Sources and Outputs
SRCS = src/rainsum.cpp  # Only rainsum.cpp is compiled, includes the header files
OBJS = $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.cpp=.o)))
DEPS = $(OBJS:.o=.d)

WASM_OUTPUT = $(DOCSDIR)/rain.wasm
JS_OUTPUT = $(DOCSDIR)/rain.cjs

# Default Target
all: directories node_modules rainsum link rainwasm

# Create Necessary Directories
directories: $(OBJDIR) $(BUILDDIR) $(WASMDIR) $(DOCSDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(WASMDIR):
	mkdir -p $(WASMDIR)

$(DOCSDIR):
	mkdir -p $(DOCSDIR)

# Install Node Modules
node_modules:
	@(test ! -d ./js/node_modules && cd js && npm install && cd ..) || :
	@(test ! -d ./scripts/node_modules && cd scripts && npm install && cd ..) || :

# Build Executable
rainsum: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(BUILDDIR)/$@ $^

# Compile Object Files
$(OBJDIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# Build WebAssembly Output
rainwasm: $(WASM_OUTPUT) $(JS_OUTPUT)

$(WASM_OUTPUT) $(JS_OUTPUT): $(SRCS) src/*.hpp
	@[ -d $(DOCSDIR) ] || mkdir -p $(DOCSDIR)
	@[ -d $(WASMDIR) ] || mkdir -p $(WASMDIR)
	$(EMCC) $(EMCCFLAGS) -o $(DOCSDIR)/rain.html $(SRCS)
	mv $(DOCSDIR)/rain.js $(JS_OUTPUT)
	cp $(WASM_OUTPUT) $(JS_OUTPUT) $(WASMDIR)
	rm $(DOCSDIR)/rain.html

# Symlink for Convenience
link:
	@ln -sf $(BUILDDIR)/rainsum

# Installation
.PHONY: install
install: rainsum
	cp $(BUILDDIR)/rainsum /usr/local/bin/

# Include Dependencies
-include $(DEPS)

# Clean Build Artifacts
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(BUILDDIR) rainsum $(WASMDIR) $(DOCSDIR) js/node_modules scripts/node_modules

