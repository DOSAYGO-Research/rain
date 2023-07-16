CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3
DEPFLAGS = -MMD -MF $(@:.o=.d)

OBJDIR = rain/obj
BUILDDIR = rain/bin
SRCS = $(wildcard src/*.cpp)
OBJS = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))
DEPS = $(OBJS:.o=.d)

WASMDIR = wasm
WASM_SOURCE = src/rainstorm.cpp
WASM_TARGET = wasm/rainstorm.js
# we need to add stringToUTF8 to exported functions rather than runtime methods because of: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#3135---040323
EMCCFLAGS = -s WASM=1 -s EXPORTED_FUNCTIONS="['_rainstormHash64', '_rainstormHash128', '_rainstormHash256', '_rainstormHash512', 'stringToUTF8', 'lengthBytesUTF8']" -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap']" -s WASM_BIGINT

all: directories rainsum link rainstorm

directories: ${OBJDIR} ${BUILDDIR} ${WASMDIR}

${OBJDIR}:
	mkdir -p ${OBJDIR}

${BUILDDIR}:
	mkdir -p ${BUILDDIR}

${WASMDIR}:
	mkdir -p ${WASMDIR}

rainsum: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/$@ $^

$(OBJDIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

rainstorm: $(WASM_TARGET)

$(WASM_TARGET): $(WASM_SOURCE)
	@[ -d wasm ] || mkdir -p wasm
	emcc $(EMCCFLAGS) -s MODULARIZE=1 -s 'EXPORT_NAME="createRainstormModule"' -o $(WASMDIR)/rainstorm.js $(WASM_SOURCE)
	#emcc $(EMCCFLAGS) -o $(WASMDIR)/rainstorm.html $(WASM_SOURCE)

link:
	ln -sf rain/bin/rainsum

.PHONY: install

install: rainsum
	cp $(BUILDDIR)/rainsum /usr/local/bin/

-include $(DEPS)

.PHONY: clean

clean:
	rm -rf $(OBJDIR) $(BUILDDIR) rainsum $(WASM_TARGET) 


